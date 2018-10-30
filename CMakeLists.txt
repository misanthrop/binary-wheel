cmake_minimum_required(VERSION 3.8)

add_library(binarywheel INTERFACE)
target_include_directories(binarywheel INTERFACE .)

get_filename_component(cppgen cpp.coffee REALPATH)
set_property(TARGET binarywheel PROPERTY cppgen ${cppgen})

function(binarywheel_generate_cpp target input)
	get_filename_component(basename ${input} NAME_WE)
	set(output ${CMAKE_CURRENT_BINARY_DIR}/${basename}.bw.hpp)
	get_target_property(cppgen binarywheel cppgen)
	add_custom_command(OUTPUT ${output} DEPENDS ${input}
		COMMAND coffee ${cppgen} ${input} ${output}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
	add_custom_target(${target} DEPENDS ${output})
	add_dependencies(${target} binarywheel)
endfunction()
