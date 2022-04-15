cmake_minimum_required(VERSION 3.5)
project(SPPCoroutines_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPCoroutines SHARED "")

set_target_properties(SPPCoroutines PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies(SPPCoroutines SPPCore )


# will need define to not create empty dll export
target_compile_definitions(SPPCoroutines
	PRIVATE
		SPP_COROUTINES_EXPORT=1)
				
target_sources(SPPCoroutines
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPCoroutines.h"
		"${CMAKE_CURRENT_LIST_DIR}/SPPCoroutines.cpp" 
		)

target_include_directories(SPPCoroutines 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )
			
target_link_libraries(SPPCoroutines 
	PUBLIC
		SPPCore 
		)

install(TARGETS SPPCoroutines DESTINATION "${SPP_ROOT}/INSTALL")



