scale = (v, vmin, vmax, min, max) -> (v - vmin)/(vmax - vmin)*(max - min) + min
stringToUtf8 = (s) -> new Uint8Array unescape(encodeURIComponent s).split('').map (c) -> c.charCodeAt 0
utf8toString = (b) -> decodeURIComponent escape String.fromCharCode.apply null, b

popCount = (bits) ->
	bits -= (bits >> 1) & 0x55555555
	bits = (bits & 0x33333333) + ((bits >> 2) & 0x33333333)
	(((bits + (bits >> 4)) & 0x0f0f0f0f) * 0x01010101) >> 24

bitMask = (bits) ->
	bits |= bits >> 1
	bits |= bits >> 2
	bits |= bits >> 4
	bits |= bits >> 8
	bits |= bits >> 16

bitsNeeded = (v) -> popCount bitMask v

class Reader
	constructor: (buf) ->
		@data = new Uint8Array buf
		@view = new DataView buf
		@cur = 0
	readBits: (count) ->
		result = 0
		pos = 0
		while count
			if not @bitsLeft
				@bits = @u8()
				@bitsLeft = 8
			bitsToRead = Math.min @bitsLeft, count
			mask = (1 << bitsToRead) - 1
			result |= (mask & @bits) << pos
			pos += bitsToRead
			count -= bitsToRead
			@bitsLeft -= bitsToRead
			@bits >>= bitsToRead
		result
	i1: -> Boolean @readBits 1
	u8: -> @view.getUint8 @cur++
	u16: -> @view.getUint16 (@cur += 2) - 2, true
	u32: -> @view.getUint32 (@cur += 4) - 4, true
	i8: -> @view.getInt8 @cur++
	i16: -> @view.getInt16 (@cur += 2) - 2, true
	i32: -> @view.getInt32 (@cur += 4) - 4, true
	f32: -> @view.getFloat32 (@cur += 4) - 4, true
	buf: (len) -> @data.subarray @cur, @cur += len

class Writer
	constructor: (sz) ->
		@data = new Uint8Array sz
		@view = new DataView @data.buffer
		@end = 0
	writeBits: (bits, count) ->
		while count
			if not @bitsLeft
				@bitsPos = @end
				@u8 0
				@bitsLeft = 8
			bitsToWrite = Math.min @bitsLeft, count
			mask = (1 << bitsToWrite) - 1
			@data[@bitsPos] |= (mask & bits) << (8 - @bitsLeft)
			@bitsLeft -= bitsToWrite
			count -= bitsToWrite
			bits >>= bitsToWrite
	i1: (v) -> @writeBits v, 1
	u8: (v) -> @view.setUint8 @end++, v
	u16: (v) -> @view.setUint16 (@end += 2) - 2, v, true
	u32: (v) -> @view.setUint32 (@end += 4) - 4, v, true
	i8: (v) -> @view.setInt8 @end++, v
	i16: (v) -> @view.setInt16 (@end += 2) - 2, v, true
	i32: (v) -> @view.setInt32 (@end += 4) - 4, v, true
	f32: (v) -> @view.setFloat32 (@end += 4) - 4, v, true
	buf: (v) ->
		@data.set v, @end
		@end += v.length
	content: -> @data.buffer.slice 0, @end

class Primitive
	constructor: (@t, @bits, @min, @max) -> @t += @bits
	bitLength: -> @bits
	unpackFrom: (r) -> r[@t]()
	packInto: (w, v) -> w[@t] v

bytesBitsNeeded = (v) ->
	if v <= 0xffff
		if v <= 0xff then 0 else 1
	else
		if v <= 0xffffffff then 2 else 3

class VarInt
	constructor: (@format) ->
	bitLength: (v) -> 2 + 8*(1 << bytesBitsNeeded v)
	unpackFrom: (r) ->
		bb = r.readBits 2
		if bb == 3 then throw new RangeError 'Length over 0xffffffff is not supported for JS'
		r[@format[bb]]()
	packInto: (w, v) ->
		bb = bytesBitsNeeded v
		if bb == 3 then throw new RangeError 'Length over 0xffffffff is not supported for JS'
		w.writeBits bb, 2
		w[@format[bb]] v

varint = new VarInt ['i8', 'i16', 'i32']
varuint = new VarInt ['u8', 'u16', 'u32']

class Utf8String
	bitLength: (v) ->
		l = stringToUtf8(v).length
		varuint.bitLength(l) + 8*l
	unpackFrom: (r) ->
		l = varuint.unpackFrom r
		utf8toString r.buf l
	packInto: (w, v) ->
		b = stringToUtf8 v
		varuint.packInto w, b.length
		w.buf b

class Scaled
	constructor: (@type, @min, @max) ->
	bitLength: -> @type.bits
	unpackFrom: (r) -> scale @type.unpackFrom(r), @type.min, @type.max, @min, @max
	packInto: (w, v) -> @type.packInto w, scale v, @min, @max, @type.min, @type.max

class Enum
	constructor: (@members) ->
		@index = {}
		for name, i in @members
			@index[name] = i
		@bits = bitsNeeded @members.length - 1
	bitLength: -> @bits
	unpackFrom: (r) -> @members[r.readBits @bits]
	packInto: (w, v) ->
		i = @index[v]
		if i?
			w.writeBits i, @bits
		else
			throw new Error "Unknown enum value #{v}"

class Optional
	constructor: (@type) ->
	bitLength: (v) -> if v? then 1 + @type.bitLength v else 1
	unpackFrom: (r) -> if r.i1() then @type.unpackFrom r
	packInto: (w, v) ->
		w.i1 v?
		if v? then @type.packInto w, v

class List
	constructor: (@type) ->
	bitLength: (v) ->
		s = varuint.bitLength v.length
		for x in v
			s += @type.bitLength x
		s
	unpackFrom: (r) ->
		l = varuint.unpackFrom r
		for i in [0 ... l]
			@type.unpackFrom r
	packInto: (w, v) ->
		varuint.packInto w, v.length
		for x in v
			@type.packInto w, x

class Struct
	constructor: (@members) ->
	bitLength: (v) ->
		s = 0
		for [name, type] in @members
			s += type.bitLength v[name]
		s
	unpackFrom: (r) ->
		result = {}
		for [name, type] in @members
			v = type.unpackFrom r
			result[name] = v if v?
		result
	packInto: (w, value) ->
		for [name, type] in @members
			type.packInto w, value[name]

byteLength = (type, v) -> (7 + type.bitLength v)//8

module.exports =
	bool: new Primitive 'i', 1, 0, 1
	int8: new Primitive 'i', 8, -0x80, 0x7f
	int16: new Primitive 'i', 16, -0x8000, 0x7fff
	int32: new Primitive 'i', 32, -0x80000000, 0x7fffffff
	uint8: new Primitive 'u', 8, 0, 0xff
	uint16: new Primitive 'u', 16, 0, 0xffff
	uint32: new Primitive 'u', 32, 0, 0xffffffff
	float32: new Primitive 'f', 32, -3.40282347e+38, 3.40282347e+38
	string: new Utf8String
	scaled: (type, min, max) -> new Scaled type, min, max
	enum: (members) -> new Enum members
	optional: (type) -> new Optional type
	list: (type) -> new List type
	struct: (members) -> new Struct if members instanceof Array then members else for key, value of members
		if value instanceof Array then [key, value[0], value[1]] else [key, value]
	Scaled: Scaled
	Enum: Enum
	Optional: Optional
	List: List
	Struct: Struct
	Reader: Reader
	Writer: Writer
	byteLength: byteLength
	unpack: (type, buf) -> type.unpackFrom new Reader buf
	pack: (type, v) ->
		w = new Writer byteLength type, v
		type.packInto w, v
		w.data.buffer
