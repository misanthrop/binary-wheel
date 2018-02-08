assert = require 'assert'
bw = require '..'
tt = require './testtypes.coffee'

data =
	empty:
		type: tt.TestStruct
		value:
			a: [], b0: false, s: '', b3: false, u: 0, i: 0, b5: false, f: 0
		bytes: new Uint8Array([0,0,0,0,0,0,0,0,0,0]).buffer
	complex:
		type: tt.TestStruct
		value:
			a: [{ name: 'a', x: 5, a: true, b: false, c: true }
				{ name: '', x: 0, v: '', a: false, b: false, c: false }
				{ name: 'xxx', x: 8, v: 'yyy', a: false, b: true, c: true } ]
			b0: false, o1: 1, s: 'str s', o2: 'o str', b3: true, u: 255, o4: 'E', i: -10, b5: false
			o6: [ 's1', 's2', 's3' ], f: 1.5
			o7: { name: 'a', x: 5, v: '', a: true, b: false, c: true }
		bytes: new Uint8Array([160,3,1,97,5,4,0,0,0,196,3,120,120,120,8,3,121,121,121,146,1,5,115,116,114
			32,115,5,111,32,115,116,114,255,41,246,3,64,2,115,49,2,115,50,2,115,51,0,0,192,63,82,1,97,5,0]).buffer
	numbers:
		type: tt.NumStruct
		value:
			i16: -12400, u16: 49312, i32: -1230340234, u32: 4012321632, s8: 0.6, s16: 0.31999694819562063
		bytes: new Uint8Array([144,207,160,192,118,127,170,182,96,43,39,239,153,235,81]).buffer
	enums:
		type: tt.EnumList
		value: [
			{e1: 'Y', e2: 'C', e3: 'D', e4: 'H' }
			{e1: 'N', e2: 'A', e3: 'A', e4: 'A' }
			{e1: 'Y', e2: 'C', e3: 'F', e4: 'I' }]
		bytes: new Uint8Array([116,3,7,64,139]).buffer
	variants:
		type: tt.VariantList
		value: [
			{type: bw.int8, value: 123}
			{type: bw.float32, value: -25.5}
			{type: bw.string, value: 'Variant String'}]
		bytes: new Uint8Array([224,3,123,0,0,204,193,0,14,86,97,114,105,97,110,116,32,83,116,114,105,110,103]).buffer

assertBuffersEqual = (a, b) ->
	a = Array.prototype.slice.call new Uint8Array a
	b = Array.prototype.slice.call new Uint8Array b
	assert.equal JSON.stringify(a), JSON.stringify(b)

for name, x of data
	describe name, -> do (x) ->
		it 'byteLength', -> assert.equal x.type.byteLength(x.value), x.bytes.byteLength
		it 'pack', -> assertBuffersEqual x.type.pack(x.value), x.bytes
		it 'unpack', -> assert.deepStrictEqual x.type.unpack(x.bytes), x.value

describe 'errors', ->
	it 'unknown enum value', ->
		assert.throws ->
			tt.TestStruct.pack
				a: [], b0: true, o1: 1, s: '', b3: true, u: 1, o4: 'F', i: -10, b5: false, o6: [], f: 1.5
			, Error, 'Unknown enum value F'
	it 'wrong variant type', ->
		assert.throws ->
			tt.VariantList.bitLength [{type: bw.uint8, value: 0}]
			, Error, 'Wrong variant type'
		assert.throws ->
			w = new Writer 32
			tt.VariantList.packInto w, [{type: bw.int16, value: 0}]
			, Error, 'Wrong variant type'
