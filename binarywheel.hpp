#pragma once
#include <numeric>
#include <type_traits>
#include <utility>
#include <functional>
#include <string>
#include <cstring>
#include <cassert>
#include <tuple>
#include <vector>
#include <optional>
#include <variant>

namespace bw
{
	template<typename T> constexpr int EnumCount = 0;
	template<typename T, typename = void> struct Type;

	struct Reader
	{
		Reader(const char* from, const char* to) : from(from), to(to) {}
		Reader(const std::vector<char>& src) : from(src.data()), to(src.data() + src.size()) {}

		size_t size() const { return to - from; }

		void read(void* dest, size_t len)
		{
			if(len > size()) throw std::range_error("Insufficient bytes in range");
			memcpy(dest, from, len);
			from += len;
		}

		uint32_t readBits(uint8_t count)
		{
			uint32_t result = 0;
			uint8_t pos = 0;
			while(count)
			{
				if(!bitsLeft)
				{
					read(&bits, 1);
					bitsLeft = 8;
				}
				uint8_t bitsToRead = std::min(bitsLeft, count);
				uint8_t mask = (uint32_t(1) << bitsToRead) - 1;
				result |= (mask & bits) << pos;
				pos += bitsToRead;
				count -= bitsToRead;
				bitsLeft -= bitsToRead;
				bits >>= bitsToRead;
			}
			return result;
		}

		template<typename T> auto unpack() { return Type<std::decay_t<T>>::unpack(*this); }

	private:
		const char* from;
		const char* to;
		uint8_t bits;
		uint8_t bitsLeft = 0;
	};

	struct Writer
	{
		Writer(std::vector<char>& dest) : dest(dest) {}

		void write(const void* src, size_t len)
		{
			dest.insert(dest.end(), (const char*)src, (const char*)src + len);
		}

		void writeBits(uint32_t bits, uint8_t count)
		{
			while(count)
			{
				if(!bitsLeft)
				{
					bitsPos = dest.size();
					dest.push_back(0);
					bitsLeft = 8;
				}
				uint8_t bitsToWrite = std::min(bitsLeft, count);
				uint8_t mask = (uint32_t(1) << bitsToWrite) - 1;
				dest[bitsPos] |= (mask & bits) << (8 - bitsLeft);
				bitsLeft -= bitsToWrite;
				count -= bitsToWrite;
				bits >>= bitsToWrite;
			}
		}

		template<typename T> void pack(const T& x) { Type<std::decay_t<T>>::packInto(*this, x); }

	private:
		std::vector<char>& dest;
		size_t bitsPos;
		uint8_t bitsLeft = 0;
	};

	constexpr uint8_t bitsNeeded(size_t max) { return 32 - __builtin_clz(max); }
	template<typename T> std::string toString(const T& x) { return Type<std::decay_t<T>>::toString(x); }
	template<typename T> constexpr size_t bitLength(const T& x) { return Type<std::decay_t<T>>::bitLength(x); }
	template<typename T> constexpr size_t byteLength(const T& x) { return (bitLength(x) + 7)/8; }
	template<typename T> T unpack(const std::vector<char>& buf) { return Reader(buf).unpack<T>(); }

	template<typename T> std::vector<char> pack(const T& x)
	{
		std::vector<char> r;
		r.reserve(byteLength(x));
		Writer(r).pack(x);
		return r;
	}

	inline float asFloat(uint32_t x) { float f; memcpy(&f, &x, sizeof(x)); return f; }
	inline float scale(float v, float vmin, float vmax, float min, float max) { return (v - vmin)/(vmax - vmin)*(max - min) + min; }

	template<typename U, uint32_t Min, uint32_t Max> struct Scaled
	{
		U value;
		Scaled() {}
		Scaled(float v) { *this = v; }
		explicit Scaled(U value) : value(value) {}
		static float min() { return asFloat(Min); }
		static float max() { return asFloat(Max); }
		operator float() const { return scale(value, std::numeric_limits<U>::min(), std::numeric_limits<U>::max(), min(), max()); }
		auto& operator=(float v) { value = scale(v, min(), max(), std::numeric_limits<U>::min(), std::numeric_limits<U>::max()); return *this; }
	};

	template<typename T, typename> struct Type
	{
		static std::string toString(const T& x) { return bw::toString(~x); }
		static constexpr size_t bitLength(const T& x) { return bw::bitLength(~x); }
		static T unpack(Reader& r) { return Type<decltype(~std::declval<T>())>::template unpackAs<T>(r); }
		static void packInto(Writer& w, const T& x) { w.pack(~x); }
	};

	template<typename U, uint32_t Min, uint32_t Max> struct Type<Scaled<U, Min, Max>>
	{
		using T = Scaled<U, Min, Max>;
		static std::string toString(const T& x) { return std::to_string((float)x); }
		static constexpr size_t bitLength(const T& x) { return bw::bitLength(x.value); }
		static T unpack(Reader& r) { return T(r.unpack<U>()); }
		static void packInto(Writer& w, const T& x) { w.pack<U>(x.value); }
	};

	template<> struct Type<bool>
	{
		static std::string toString(const bool& x) { return x ? "+" : "-"; }
		static constexpr size_t bitLength(const bool&) { return 1; }
		static bool unpack(Reader& r) { return r.readBits(1); }
		static void packInto(Writer& w, const bool& x) { w.writeBits(x, 1); }
	};

	template<typename T> struct Type<T, std::enable_if_t<std::is_enum_v<T>, void>>
	{
		static constexpr uint8_t bits = bitsNeeded(EnumCount<T> - 1);
		static std::string toString(const T& x) { return std::to_string((uint8_t)x); }
		static constexpr size_t bitLength(const T&) { return bits; }
		static T unpack(Reader& r) { return (T)r.readBits(bits); }
		static void packInto(Writer& w, const T& x) { w.writeBits((uint32_t)x, bits); }
	};

	template<typename T> struct Type<T, std::enable_if_t<std::is_arithmetic_v<T>, void>>
	{
		static std::string toString(const T& x) { return std::to_string(x); }
		static constexpr size_t bitLength(const T&) { return 8*sizeof(T); }
		static T unpack(Reader& r) { T x; r.read(&x, sizeof(T)); return x; }
		static void packInto(Writer& w, const T& x) { w.write(&x, sizeof(T)); }
	};

	template<typename... Args> struct Type<std::variant<Args...>>
	{
		using T = std::variant<std::decay_t<Args>...>;
		static constexpr uint8_t bits = bitsNeeded(std::variant_size_v<T> - 1);

		static std::string toString(const T& x) { return std::visit([](auto&& v) { return bw::toString(v); }, x); }
		static size_t bitLength(const T& x) { return bits + std::visit([](auto&& v) { return bw::bitLength(v); }, x); }

		template<size_t I = 0> static T unpackByType(Reader& r, size_t type)
		{
			if constexpr (I < std::variant_size_v<T>)
				return I != type ? unpackByType<I + 1>(r, type) :
					T(r.unpack<std::variant_alternative_t<I, T>>());
			assert(false);
		}

		static T unpack(Reader& r) { return unpackByType<>(r, r.readBits(bits)); }

		static void packInto(Writer& w, const T& x)
		{
			w.writeBits(x.index(), bits);
			std::visit([&](const auto& v) { w.pack(v); }, x);
		}
	};

	using VarInt = std::variant<uint8_t, uint16_t, uint32_t, uint64_t>;

	inline constexpr VarInt toVarInt(size_t v)
	{
		return v <= 0xffff
			? (v <= 0xff ? VarInt(uint8_t(v)) : VarInt(uint16_t(v)))
			: (v <= 0xffffffff ? VarInt(uint32_t(v)) : VarInt(uint64_t(v)));
	}

	inline constexpr size_t fromVarInt(const VarInt& x)
	{
		return std::visit([](const auto& v) { return (size_t)v; }, x);
	}

	template<> struct Type<std::string>
	{
		static std::string toString(const std::string& x) { return '\'' + x + '\''; }
		static size_t bitLength(const std::string& x) { return bw::bitLength(toVarInt(x.size())) + 8*x.size(); }

		static std::string unpack(Reader& r)
		{
			size_t len = fromVarInt(r.unpack<VarInt>());
			std::string x;
			char s[1024];
			while(len)
			{
				size_t n = std::min(len, sizeof(s));
				r.read(s, n);
				x.insert(x.end(), s, s + n);
				len -= n;
			}
			return x;
		}

		static void packInto(Writer& w, const std::string& x)
		{
			w.pack(toVarInt(x.size()));
			w.write(x.data(), x.size());
		}
	};

	template<typename T> struct Type<std::optional<T>>
	{
		static std::string toString(const std::optional<T>& x) { return x ? bw::toString(*x) : "?"; }
		static constexpr size_t bitLength(const std::optional<T>& x) { return 1 + (x ? bw::bitLength(*x) : 0); }
		static std::optional<T> unpack(Reader& r) { return r.readBits(1) ? std::optional(r.unpack<T>()) : std::nullopt; }

		static void packInto(Writer& w, const std::optional<T>& x)
		{
			w.writeBits(bool(x), 1);
			if(x) w.pack(*x);
		}
	};

	template<typename T> struct Type<std::vector<T>>
	{
		static std::string toString(const std::vector<T>& x)
		{
			std::string result = "[ ";
			for(const auto& m : x) result += bw::toString(m) += ' ';
			return result += ']';
		}

		static size_t bitLength(const std::vector<T>& x)
		{
			size_t s = bw::bitLength(toVarInt(x.size()));
			for(const T& m : x) s += bw::bitLength(m);
			return s;
		}

		static std::vector<T> unpack(Reader& r)
		{
			size_t len = fromVarInt(r.unpack<VarInt>());
			std::vector<T> x;
			while(len--) x.push_back(r.unpack<T>());
			return x;
		}

		static void packInto(Writer& w, const std::vector<T>& x)
		{
			w.pack(toVarInt(x.size()));
			for(const auto& v : x) w.pack(v);
		}
	};

	template<typename... Args> struct Type<std::tuple<Args...>>
	{
		static std::string toString(const std::tuple<Args...>& x) { return std::apply([](const auto&... args) { return "( " + ((bw::toString(args) + ' ') + ...) + ')'; }, x); }
		static constexpr size_t bitLength(const std::tuple<Args...>& x) { return std::apply([](const auto&... args) { return (bw::bitLength(args) + ...); }, x); }
		template<typename T> static T unpackAs(Reader& r) { return T{r.unpack<Args>()...}; }
		static auto unpack(Reader& r) { return unpackAs<std::tuple<std::decay_t<Args>...>>(r); }
		static void packInto(Writer& w, const std::tuple<Args...>& x) { std::apply([&](const auto&... args) { (w.pack(args), ...); }, x); }
	};
}
