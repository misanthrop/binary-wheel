bw = require '.'

bw.bool.name = 'bool'
bw.int8.name = 'int8_t'
bw.uint8.name = 'uint8_t'
bw.int16.name = 'int16_t'
bw.uint16.name = 'uint16_t'
bw.int32.name = 'int32_t'
bw.uint32.name = 'uint32_t'
bw.float32.name = 'float'
bw.string.name = 'std::string'

capitalizeFirstLetter = (s) -> s[0].toUpperCase() + s.substring 1

floatAsUint32 = (v) ->
	view = new DataView new ArrayBuffer 4
	view.setFloat32 0, v
	view.getUint32 0

hex = (v) -> "0x#{v.toString 16}"

templateFloat = (v) -> if v then "#{hex floatAsUint32 v} /*#{v}*/" else 0

generate = (publicTypes, namespace = '') ->
	allTypes = {}

	decls =
		forward: []
		other: []
		adapters: []

	newName = (nameHint) ->
		name = nameHint
		i = 0
		while allTypes[name]
			name = "#{nameHint}#{i += 1}"
		name

	register = (type, hint, pub = false) ->
		type.pub or= pub or type instanceof bw.Enum
		if not type.registered
			type.registered = true
			name = type.typename? hint
			if not type.name?
				allTypes[type.name = name] = type
			if adapt = type.adapter?()
				decls.adapters.push adapt
		type.name

	declare = (type) -> if not type.declared and type.pub
		type.declared = true
		if deps = type.dependencies?()
			for n, t of deps
				declare t
		decl = type.declaration?() ? (type.spec and type.name != type.spec and "using #{type.name} = #{type.spec}" or "")
		decls[if not deps then 'forward' else 'other'].push decl

	bw.List::typename = (hint) -> @spec = "std::vector<#{register @type, hint, true}>"
	bw.Optional::typename = (hint) -> @spec = "std::optional<#{register @type, hint, true}>"
	bw.Variant::typename = (hint) -> @spec = "std::variant<#{(register type, hint, true for type in @types).join ', '}>"
	bw.Scaled::typename = (hint = 'Scaled') -> @spec = "bw::Scaled<#{@type.name}, #{templateFloat @min}, #{templateFloat @max}>"
	bw.Enum::typename = (hint = 'Enum') -> newName hint

	bw.Struct::typename = (hint = 'Struct') ->
		for [name, type] in @members
			throw new Error "Undefined type of #{hint}.#{name}" if not type
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

	bw.Enum::declaration = -> "enum class #{@name} : uint8_t { #{@members.join ', '} }"
	bw.Enum::adapter = -> "template<> constexpr int EnumCount<#{namespace}::#{@name}> = #{@members.length};"

	cppValue = (value) ->
		if value instanceof Array
			"{ #{value.map((v) -> cppValue v).join ', '} }"
		else if value instanceof String
			'"' + value + '"'
		else
			value

	valueAssignment = (value) ->
		if value?
			" = #{cppValue value}"
		else
			""

	bw.Struct::declaration = (ident = '') ->
		members = @members.map ([name, type, value]) ->
			"#{if type.pub or not type.declaration? then type.name else type.declaration ident + '\t'} #{name}#{valueAssignment value};"
		params = @members.map ([name]) -> name
		"""
		struct#{if @pub then ' ' + @name else ''}
		#{ident}{
		#{ident}	#{members.join '\n\t' + ident}
		#{ident}	auto operator~() const { return std::forward_as_tuple(#{params.join ', '}); }
		#{ident}	auto operator~() { return std::forward_as_tuple(#{params.join ', '}); }
		#{ident}}
		"""

	for name, type of publicTypes
		allTypes[type.name = name] = type

	for name, type of publicTypes
		register type, name, true

	for name, type of allTypes
		declare type

	forwardStructs = for name, type of allTypes when type.pub and type instanceof bw.Struct
		"struct #{name}"

	decls.forward = forwardStructs.concat decls.forward

	for t in ['forward', 'other']
		decls[t] = decls[t].filter (x) -> x

	"""
	#pragma once
	#include <binarywheel.hpp>

	#{if namespace then 'namespace ' + namespace + '\n{' else ''}
	#{decls.forward.join ';\n'};

	#{decls.other.join ';\n\n'};
	#{if namespace then '}' else ''}

	namespace bw
	{
	#{decls.adapters.join '\n'}
	}

	"""

module.exports = {generate}
