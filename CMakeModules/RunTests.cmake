# runs test binaries given by parameter

macro(run_tests  _tests)
	foreach(test ${${_tests}})
		add_custom_command(TARGET ${test} POST_BUILD
						   COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${test})
	endforeach()
endmacro(run_tests)
