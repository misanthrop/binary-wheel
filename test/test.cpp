#include <iostream>
#include <vector>
#include <binarywheel.hpp>
#include <testtypes.hpp>
#include "lest.hpp"

using namespace std;

bool operator==(const Nested& a, const Nested& b) { return ~a == ~b; }
bool operator==(const EnumStruct& a, const EnumStruct& b) { return ~a == ~b; }
bool operator==(const NumStruct& a, const NumStruct& b) { return ~a == ~b; }
bool operator==(const TestStruct& a, const TestStruct& b) { return ~a == ~b; }

const TestStruct t0{};

const TestStruct t1 =
{
	{
		Nested{"a"s, 5, nullopt, 1,0,1},
		Nested{""s, 0, ""s, 0,0,0},
		Nested{"xxx"s, 8, "yyy"s, 0,1,1}
	},
	false, 1, "str s"s, "o str"s, true, 255, Enum::E, -10, false,
	vector<string>{"s1"s, "s2"s, "s3"s}, 1.5,
	Nested{"a"s, 5, ""s, 1,0,1}
};

const string t0s = "( [ ] - ? '' ? - 0 ? 0 - ? 0.000000 ? )";
const string t1s = "( [ ( 'a' 5 ? + - + ) ( '' 0 '' - - - ) ( 'xxx' 8 'yyy' - + + ) ] - 1 'str s' 'o str' + 255 4 -10 - [ 's1' 's2' 's3' ] 1.500000 ( 'a' 5 '' + - + ) )"s;
const vector<char> t0b = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

const vector<uint8_t> t1ub =
{
	160,3,1,97,5,4,0,0,0,196,3,120,120,120,8,3,121,121,121,146,1,5,115,116,114,32,115,5,
	111,32,115,116,114,255,41,246,3,64,2,115,49,2,115,50,2,115,51,0,0,192,63,82,1,97,5,0
};

const vector<char> t1b(t1ub.begin(), t1ub.end());

const vector<EnumStruct> e1 = {
	{E1::Y, E2::C, E3::D, E4::H },
	{E1::N, E2::A, E3::A, E4::A },
	{E1::Y, E2::C, E3::F, E4::I }};

const vector<uint8_t> e1ub = {116,3,7,64,139};
const vector<char> e1b(e1ub.begin(), e1ub.end());

const NumStruct n1 = { -12400, 49312, -1230340234, 4012321632, 0.6, 0.31999694819562063 };
const vector<uint8_t> n1ub = {144,207,160,192,118,127,170,182,96,43,39,239,153,235,81};
const vector<char> n1b(n1ub.begin(), n1ub.end());

const lest::test tests[] =
{
	CASE("empty")
	{
		EXPECT(bw::toString(t0) == t0s);
		EXPECT(bw::byteLength(t0) == t0b.size());
		EXPECT(bw::pack(t0) == t0b);
		EXPECT(bw::unpack<TestStruct>(t0b) == t0);
	},

	CASE("enums")
	{
		EXPECT(bw::byteLength(e1) == e1b.size());
		EXPECT(bw::pack(e1) == e1b);
		EXPECT(bw::unpack<vector<EnumStruct>>(e1b) == e1);
	},

	CASE("numbers")
	{
		EXPECT(bw::byteLength(n1) == n1b.size());
		EXPECT(bw::pack(n1) == n1b);
		EXPECT(bw::unpack<NumStruct>(n1b) == n1);
	},

	CASE("complex")
	{
		EXPECT(bw::toString(t1) == t1s);
		EXPECT(bw::byteLength(t1) == t1b.size());
		EXPECT(bw::pack(t1) == t1b);
		EXPECT(bw::unpack<TestStruct>(t1b) == t1);
	},

	CASE("other")
	{
		TestStruct x;
		bw::unpackFrom(bw::pack(t0), x);
		EXPECT(x == t0);
		bw::unpackFrom(bw::pack(t1), x);
		EXPECT(x == t1);
	}
};

int main(int argc, char* argv[]) { return lest::run(tests, argc, argv); }
