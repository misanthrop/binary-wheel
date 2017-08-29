path = require 'path'
bw = require '.'

publicTypes = require path.resolve process.argv[2]
allTypes = {}

decls =
	forward: []
	other: []

newName = (nameHint) ->
	name = nameHint
	i = 0
	while allTypes[name]
		name = "#{nameHint}#{i += 1}"
	name

register = (type, hint) ->
	name = type.typename? hint
	if not type.name?
		allTypes[type.name = name] = type
	type.name

declare = (type) -> if not type.declared
	type.declared = true
	if deps = type.dependencies?()
		for n, t of deps
			declare t
	noDeps = type instanceof bw.Enum or type instanceof bw.Scaled
	decl = type.declaration?() ? (type.spec and type.name != type.spec and "using #{type.name} = #{type.spec};" or "")
	decls[if noDeps then 'forward' else 'other'].push decl

capitalizeFirstLetter = (s) -> s[0].toUpperCase() + s.substring 1

floatAsUint32 = (v) ->
	view = new DataView new ArrayBuffer 4
	view.setFloat32 0, v
	view.getUint32 0

hex = (v) -> "0x#{v.toString 16}"

bw.bool.name = 'bool'
bw.int8.name = 'int8_t'
bw.uint8.name = 'uint8_t'
bw.int16.name = 'int16_t'
bw.uint16.name = 'uint16_t'
bw.int32.name = 'int32_t'
bw.uint32.name = 'uint32_t'
bw.float32.name = 'float'
bw.string.name = 'std::string'

bw.List::typename = (hint) -> @spec = "std::vector<#{register @type, hint}>"
bw.Optional::typename = (hint) -> @spec = "std::optional<#{register @type, hint}>"
bw.Scaled::typename = (hint = 'Scaled') -> @spec = "bw::Scaled<#{@type.name}, #{hex floatAsUint32 @min} /*#{@min}*/, #{hex floatAsUint32 @max} /*#{@max}*/>"
bw.Enum::typename = (hint = 'Enum') -> newName hint

bw.Struct::typename = (hint = 'Struct') ->
	for [name, type] in @members
		register type, capitalizeFirstLetter name
	newName hint

bw.Struct::dependencies = ->
	deps = {}
	if @inStack
		throw new Error "Circular dependency detected: #{@name}"
	@inStack = true
	for [name, type] in @members
		if type.dependencies?
			deps[type.name] = type
			Object.assign deps, type.dependencies()
	delete @inStack
	deps

bw.Enum::declaration = -> """
	enum class #{@name} : uint8_t { #{@members.join ', '} };
	namespace bw { template<> constexpr int EnumCount<#{@name}> = #{@members.length}; }
	"""

bw.Struct::declaration = ->
	members = @members.map ([name, type]) -> "#{type.name} #{name};"
	memberNames = @members.map ([name]) -> name
	"""
	struct #{@name} {
		#{members.join '\n\t'}
		auto operator~() const { return std::forward_as_tuple(#{memberNames.join ', '}); }
		auto operator~() { return std::forward_as_tuple(#{memberNames.join ', '}); }
	};
	"""

for name, type of publicTypes
	allTypes[type.name = name] = type

for name, type of publicTypes
	register type, name

for name, type of allTypes
	declare type

decls.structs = for name, type of allTypes when type instanceof bw.Struct
	"struct #{name};"

decls.forward = decls.forward.filter (x) -> x
decls.other = decls.other.filter (x) -> x

process.stdout.write """
	#pragma once
	#include <binarywheel.hpp>

	#{decls.forward.join '\n\n'}

	#{decls.structs.join '\n'}

	#{decls.other.join '\n\n'}

	"""
