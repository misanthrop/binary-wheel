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
#if __cplusplus <= 201402L
#include <experimental/optional>
namespace std { using namespace std::experimental; }
#else
#include <optional>
#endif

namespace bw
{
	template<template<class...> class TT, class X> constexpr bool is_same_template = false;
	template<template<class...> class TT, class... P> constexpr bool is_same_template<TT, TT<P...>> = true;

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

	private:
		std::vector<char>& dest;
		size_t bitsPos;
		uint8_t bitsLeft = 0;
	};

	template<class T> constexpr int EnumCount = 0;
	template<class T, class = void> struct Type;
	template<class T> auto asTuple(T&& x) { return std::forward<T>(x); }

	template<class T> std::string toString(const T& x) { return Type<std::decay_t<T>>::toString(x); }
	template<class T> size_t bitLength(const T& x) { return Type<std::decay_t<T>>::bitLength(x); }
	template<class T> void unpackFrom(Reader& r, T&& x) { Type<std::decay_t<T>>::unpackFrom(r, std::forward<T>(x)); }
	template<class T> void unpackFrom(Reader& r, T& x) { Type<std::decay_t<T>>::unpackFrom(r, x); }
	template<class T> void packInto(Writer& w, const T& x) { Type<std::decay_t<T>>::packInto(w, x); }

	template<class T> size_t byteLength(const T& x) { return (bitLength(x) + 7)/8; }
	template<class T> void unpackFrom(const std::vector<char>& v, T& x) { Reader r(v); unpackFrom(r, x); }
	template<class T, class B> T unpack(B& v) { T x; unpackFrom(v, x); return x; }
	template<class T> void packInto(std::vector<char>& v, const T& x) { Writer w(v); packInto(w, x); }
	template<class T> std::vector<char> pack(const T& x) { std::vector<char> r; r.reserve(byteLength(x)); packInto(r, x); return r; }

	inline float asFloat(uint32_t x) { float f; memcpy(&f, &x, sizeof(x)); return f; }
	inline float scale(float v, float vmin, float vmax, float min, float max) { return (v - vmin)/(vmax - vmin)*(max - min) + min; }

	template<class U, uint32_t Min, uint32_t Max> struct Scaled
	{
		U value;
		Scaled() {}
		Scaled(float v) { *this = v; }
		static float min() { return asFloat(Min); }
		static float max() { return asFloat(Max); }
		operator float() const { return scale(value, std::numeric_limits<U>::min(), std::numeric_limits<U>::max(), min(), max()); }
		auto& operator=(float v) { value = scale(v, min(), max(), std::numeric_limits<U>::min(), std::numeric_limits<U>::max()); return *this; }
		auto operator~() const { return value; }
		auto& operator~() { return value; }
	};

	template<> struct Type<bool>
	{
		static std::string toString(const bool& x) { return x ? "+" : "-"; }
		static size_t bitLength(const bool&) { return 1; }
		static void unpackFrom(Reader& r, bool& x) { x = r.readBits(1); }
		static void packInto(Writer& w, const bool& x) { w.writeBits(x, 1); }
	};

	template<class T> struct Type<T, std::enable_if_t<std::is_enum<T>::value, void>>
	{
		static constexpr uint8_t bits = 32 - __builtin_clz(EnumCount<T> - 1);
		static std::string toString(const T& x) { return std::to_string((uint8_t)x); }
		static size_t bitLength(const T&) { return bits; }
		static void unpackFrom(Reader& r, T& x) { x = (T)r.readBits(bits); }
		static void packInto(Writer& w, const T& x) { w.writeBits((uint32_t)x, bits); }
	};

	template<class T> struct Type<T, std::enable_if_t<std::is_arithmetic<T>::value, void>>
	{
		static std::string toString(const T& x) { return std::to_string(x); }
		static size_t bitLength(const T&) { return 8*sizeof(T); }
		static void unpackFrom(Reader& r, T& x) { r.read(&x, sizeof(T)); }
		static void packInto(Writer& w, const T& x) { w.write(&x, sizeof(T)); }
	};

	struct VarInt
	{
		static uint8_t bytesBitsNeeded(size_t v) { return v <= 0xffff ? (v <= 0xff ? 0 : 1) : (v <= 0xffffffff ? 2 : 3); }
		static std::string toString(size_t x) { return std::to_string(x); }
		static size_t bitLength(size_t x) { return 2 + 8*(size_t(1) << bytesBitsNeeded(x)); }

		static size_t unpack(Reader& r)
		{
			switch(r.readBits(2))
			{
				case 0: return bw::unpack<uint8_t>(r);
				case 1: return bw::unpack<uint16_t>(r);
				case 2: return bw::unpack<uint32_t>(r);
				case 3: return bw::unpack<uint64_t>(r);
			}
		}

		static void packInto(Writer& w, size_t x)
		{
			uint8_t bb = bytesBitsNeeded(x);
			w.writeBits(bb, 2);
			switch(bb)
			{
				case 0: Type<uint8_t>::packInto(w, x); break;
				case 1: Type<uint16_t>::packInto(w, x); break;
				case 2: Type<uint32_t>::packInto(w, x); break;
				case 3: Type<uint64_t>::packInto(w, x); break;
			}
		}
	};

	template<> struct Type<std::string>
	{
		static std::string toString(const std::string& x) { return '\'' + x + '\''; }
		static size_t bitLength(const std::string& x) { return VarInt::bitLength(x.size()) + 8*x.size(); }

		static void unpackFrom(Reader& r, std::string& x)
		{
			size_t len = VarInt::unpack(r);
			x.clear();
			char s[1024];
			while(len)
			{
				size_t n = std::min(len, sizeof(s));
				r.read(s, n);
				x.insert(x.end(), s, s + n);
				len -= n;
			}
		}

		static void packInto(Writer& w, const std::string& x)
		{
			VarInt::packInto(w, x.size());
			w.write(x.data(), x.size());
		}
	};

	template<class T> struct Type<std::optional<T>>
	{
		static std::string toString(const std::optional<T>& x) { return x ? bw::toString(*x) : "?"; }
		static size_t bitLength(const std::optional<T>& x) { return 1 + (x ? bw::bitLength(*x) : 0); }

		static void unpackFrom(Reader& r, std::optional<T>& x)
		{
			if(r.readBits(1))
			{
				x = T();
				bw::unpackFrom(r, *x);
			}
			else x = std::nullopt;
		}

		static void packInto(Writer& w, const std::optional<T>& x)
		{
			w.writeBits(bool(x), 1);
			if(x) bw::packInto(w, *x);
		}
	};

	template<class T> struct Type<std::vector<T>>
	{
		static std::string toString(const std::vector<T>& x)
		{
			std::string result = "[ ";
			for(const auto& m : x) result += bw::toString(m) += ' ';
			return result += ']';
		}

		static size_t bitLength(const std::vector<T>& x)
		{
			size_t s = VarInt::bitLength(x.size());
			for(const T& m : x) s += bw::bitLength(m);
			return s;
		}

		static void unpackFrom(Reader& r, std::vector<T>& x)
		{
			size_t len = VarInt::unpack(r);
			x.clear();
			while(len--)
			{
				x.emplace_back();
				bw::unpackFrom(r, x.back());
			}
		}

		static void packInto(Writer& w, const std::vector<T>& x)
		{
			VarInt::packInto(w, x.size());
			for(const auto& v : x) bw::packInto(w, v);
		}
	};

	inline std::string toStringArgs() { return ""; }
	template<class Arg, class... Args> inline std::string toStringArgs(const Arg& first, const Args&... rest) { return bw::toString(first) + ' ' + toStringArgs(rest...); }

	inline size_t sumArgs() { return 0; }
	template<class Arg, class... Args> inline size_t sumArgs(Arg first, Args... rest) { return first + sumArgs(rest...); }

	inline void unpackArgs(Reader&) {}
	template<class Arg, class... Args> inline void unpackArgs(Reader& r, Arg& first, Args&... rest) { unpackFrom(r, first); unpackArgs(r, rest...); }

	inline void packArgs(Writer&) {}
	template<class Arg, class... Args> inline void packArgs(Writer& w, const Arg& first, const Args&... rest) { packInto(w, first); packArgs(w, rest...); }

	template<class T, size_t... I> std::string toStringTuple(const T& x, std::index_sequence<I...>) { return toStringArgs(std::get<I>(x)...); }
	template<class T, size_t... I> size_t bitLengthTuple(const T& x, std::index_sequence<I...>) { return sumArgs(bitLength(std::get<I>(x))...); }
	template<class T, size_t... I> void unpackTuple(Reader& r, T&& x, std::index_sequence<I...>) { unpackArgs(r, std::get<I>(std::forward<T>(x))...); }
	template<class T, size_t... I> void packTuple(Writer& w, const T& x, std::index_sequence<I...>) { packArgs(w, std::get<I>(x)...); }

	template<class... Args> struct Type<std::tuple<Args...>>
	{
		using T = std::tuple<Args...>;

		static std::string toString(const T& x) { return "( " + toStringTuple(x, std::make_index_sequence<std::tuple_size<T>::value>()) + ')'; }
		static size_t bitLength(const T& x) { return bitLengthTuple(x, std::make_index_sequence<std::tuple_size<T>::value>()); }
		static void unpackFrom(Reader& r, T&& x) { unpackTuple(r, std::forward<T>(x), std::make_index_sequence<std::tuple_size<T>::value>()); }
		static void packInto(Writer& w, const T& x) { packTuple(w, x, std::make_index_sequence<std::tuple_size<T>::value>()); }
	};

	template<class T> struct Type<T, std::enable_if_t<std::is_class<T>::value
		&& !is_same_template<std::optional, T>
		&& !is_same_template<std::vector, T>
		&& !is_same_template<std::tuple, T>, void>>
	{
		static std::string toString(const T& x) { return bw::toString(~x); }
		static size_t bitLength(const T& x) { return bw::bitLength(~x); }
		static void unpackFrom(Reader& r, T& x) { bw::unpackFrom(r, ~x); }
		static void packInto(Writer& w, const T& x) { bw::packInto(w, ~x); }
	};
}
