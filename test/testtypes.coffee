bw = require '..'

Enum = bw.enum ['A', 'B', 'C', 'D', 'E']

Nested = bw.struct
	name: bw.string
	x: bw.uint8
	v: bw.optional bw.string
	a: bw.bool
	b: bw.bool
	c: bw.bool

TestStruct = bw.struct [
	['a', bw.list Nested]
	['b0', bw.bool]
	['o1', bw.optional bw.uint8]
	['s', bw.string]
	['o2', bw.optional bw.string]
	['b3', bw.bool]
	['u', bw.uint8]
	['o4', bw.optional Enum]
	['i', bw.int8]
	['b5', bw.bool]
	['o6', bw.optional bw.list bw.string]
	['f', bw.float32]
	['o7', bw.optional Nested]]

NumStruct = bw.struct [
	['i16', bw.int16]
	['u16', bw.uint16]
	['i32', bw.int32]
	['u32', bw.uint32]
	['s8', bw.scaled bw.uint8, 0, 1]
	['s16', bw.scaled bw.uint16, 0, 1]]

EnumStruct = bw.struct [
	['e1', bw.enum ['N', 'Y']]
	['e2', bw.enum ['A', 'B', 'C']]
	['e3', bw.enum ['A', 'B', 'C', 'D', 'E', 'F']]
	['e4', bw.enum ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I']]]

EnumList = bw.list EnumStruct

VariantList = bw.list bw.variant [bw.int8, bw.int32, bw.float32, bw.string]

module.exports = {
	Enum, Nested, TestStruct, NumStruct, EnumStruct, EnumList, VariantList}
