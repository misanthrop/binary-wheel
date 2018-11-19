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
#include <limits>

namespace bw
{
	template<typename T> struct Type;

	struct Reader
	{
		Reader(const char* from, const char* to) noexcept : from(from), to(to) {}
		Reader(std::string_view src) noexcept : from(src.data()), to(src.data() + src.size()) {}
		Reader(const std::vector<char>& src) noexcept : from(src.data()), to(src.data() + src.size()) {}

		size_t size() const noexcept { return to - from; }

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
		Writer(std::vector<char>& dest) noexcept : dest(dest) {}

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

	template<typename T> struct Type
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

	template<typename T, uint8_t Bits> struct BitsType
	{
		static constexpr uint8_t bits = Bits;
		static constexpr size_t bitLength(const T&) { return bits; }
		static bool unpack(Reader& r) { return static_cast<T>(r.readBits(bits)); }
		static void packInto(Writer& w, const T& x) { w.writeBits(static_cast<uint32_t>(x), bits); }
	};

	template<typename T, int Count> struct EnumType : BitsType<T, bitsNeeded(Count - 1)>
	{
		static constexpr int count = Count;
	};

	template<> struct Type<bool> : BitsType<bool, 1>
	{
		static std::string toString(const bool& x) { return x ? "+" : "-"; }
	};

	template<typename T> struct NumberType
	{
		static std::string toString(const T& x) { return std::to_string(x); }
		static constexpr size_t bitLength(const T&) { return 8*sizeof(T); }
		static T unpack(Reader& r) { T x; r.read(&x, sizeof(T)); return x; }
		static void packInto(Writer& w, const T& x) { w.write(&x, sizeof(T)); }
	};

	template<> struct Type<int8_t> : NumberType<int8_t> {};
	template<> struct Type<int16_t> : NumberType<int16_t> {};
	template<> struct Type<int32_t> : NumberType<int32_t> {};
	template<> struct Type<int64_t> : NumberType<int64_t> {};
	template<> struct Type<uint8_t> : NumberType<uint8_t> {};
	template<> struct Type<uint16_t> : NumberType<uint16_t> {};
	template<> struct Type<uint32_t> : NumberType<uint32_t> {};
	template<> struct Type<uint64_t> : NumberType<uint64_t> {};
	template<> struct Type<float> : NumberType<float> {};

	namespace varint
	{
		static constexpr uint8_t bytesBitsNeeded(size_t v) { return v <= 0xffff ? (v <= 0xff ? 0 : 1) : (v <= 0xffffffff ? 2 : 3); }
		static constexpr size_t bitLength(size_t x) { return 2 + 8*(size_t(1) << bytesBitsNeeded(x)); }

		static size_t unpack(Reader& r)
		{
			switch(r.readBits(2))
			{
				case 0: return r.unpack<uint8_t>();
				case 1: return r.unpack<uint16_t>();
				case 2: return r.unpack<uint32_t>();
			}
			return r.unpack<uint64_t>();
		}

		static void packInto(Writer& w, size_t x)
		{
			uint8_t bb = bytesBitsNeeded(x);
			w.writeBits(bb, 2);
			switch(bb)
			{
				case 0: return Type<uint8_t>::packInto(w, x);
				case 1: return Type<uint16_t>::packInto(w, x);
				case 2: return Type<uint32_t>::packInto(w, x);
				case 3: return Type<uint64_t>::packInto(w, x);
			}
		}
	};

	template<> struct Type<std::string>
	{
		static std::string toString(const std::string& x) { return '\'' + x + '\''; }
		static size_t bitLength(const std::string& x) { return varint::bitLength(x.size()) + 8*x.size(); }

		static std::string unpack(Reader& r)
		{
			size_t len = varint::unpack(r);
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
			varint::packInto(w, x.size());
			w.write(x.data(), x.size());
		}
	};

	template<typename T> struct Type<std::optional<T>>
	{
		static std::string toString(const std::optional<T>& x) { return x ? bw::toString(*x) : "?"; }
		static constexpr size_t bitLength(const std::optional<T>& x) { return 1 + (x ? bw::bitLength(*x) : 0); }
		static std::optional<T> unpack(Reader& r) { return r.readBits(1) ? std::make_optional(r.unpack<T>()) : std::nullopt; }

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
			size_t s = varint::bitLength(x.size());
			for(const T& m : x) s += bw::bitLength(m);
			return s;
		}

		static std::vector<T> unpack(Reader& r)
		{
			size_t len = varint::unpack(r);
			std::vector<T> x;
			while(len--) x.push_back(r.unpack<T>());
			return x;
		}

		static void packInto(Writer& w, const std::vector<T>& x)
		{
			varint::packInto(w, x.size());
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
