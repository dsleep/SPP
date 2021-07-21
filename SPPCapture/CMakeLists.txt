cmake_minimum_required(VERSION 3.5)
project(SPPCapture_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPCapture SHARED "")

set_target_properties(SPPCapture PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPCapture SPPCore )

# will need define to not create empty dll export
target_compile_definitions(SPPCapture
	PRIVATE
		SPP_CAPTURE_EXPORT=1)
				
target_sources(SPPCapture
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPCapture.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPCapture.h"				
		)

target_include_directories(SPPCapture 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )
			
target_link_libraries( SPPCapture 
	PUBLIC
		SPPCore )

install(TARGETS SPPCapture DESTINATION "${SPP_ROOT}/INSTALL")



