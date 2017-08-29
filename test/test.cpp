#include <iostream>
#include <vector>
#include <bandit/bandit.h>
#include <binarywheel.hpp>
#include <testtypes.hpp>

using namespace std;
using namespace snowhouse;
using namespace bandit;

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

go_bandit([]()
{
	describe("empty", []()
	{
		it("toString", []() { AssertThat(bw::toString(t0), Equals(t0s)); });
		it("byteLength", []() { AssertThat(bw::byteLength(t0), Equals(t0b.size())); });
		it("pack", []() { AssertThat(bw::pack(t0), Equals(t0b)); });
		it("unpack", []() { AssertThat(bw::unpack<TestStruct>(t0b), Equals(t0)); });
	});

	describe("enums", []()
	{
		it("byteLength", []() { AssertThat(bw::byteLength(e1), Equals(e1b.size())); });
		it("pack", []() { AssertThat(bw::pack(e1), Equals(e1b)); });
		it("unpack", []() { AssertThat(bw::unpack<vector<EnumStruct>>(e1b), Equals(e1)); });
	});

	describe("numbers", []()
	{
		it("byteLength", []() { AssertThat(bw::byteLength(n1), Equals(n1b.size())); });
		it("pack", []() { AssertThat(bw::pack(n1), Equals(n1b)); });
		it("unpack", []() { AssertThat(bw::unpack<NumStruct>(n1b), Equals(n1)); });
	});

	describe("complex", []()
	{
		it("toString", []() { AssertThat(bw::toString(t1), Equals(t1s)); });
		it("byteLength", []() { AssertThat(bw::byteLength(t1), Equals(t1b.size())); });
		it("pack", []() { AssertThat(bw::pack(t1), Equals(t1b)); });
		it("unpack", []() { AssertThat(bw::unpack<TestStruct>(t1b), Equals(t1)); });
	});

	describe("other", []()
	{
		it("write parse", []()
		{
			TestStruct x;
			bw::unpackFrom(bw::pack(t0), x);
			AssertThat(x, Equals(t0));
			bw::unpackFrom(bw::pack(t1), x);
			AssertThat(x, Equals(t1));
		});
	});
});

int main(int argc, char* argv[]) { return run(argc, argv); }
