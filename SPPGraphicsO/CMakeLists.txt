cmake_minimum_required(VERSION 3.5)
project(SPPGraphicsO VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPGraphicsO SHARED "")

set_target_properties(SPPGraphicsO PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPGraphicsO SPPCore SPPObject SPPMath SPPSceneO SPPGraphics )


# will need define to not create empty dll export
target_compile_definitions(SPPGraphicsO
	PRIVATE
		SPP_GRAPHICSO_EXPORT=1)
				
target_sources(SPPGraphicsO
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPGraphicsO.h"
		"${CMAKE_CURRENT_LIST_DIR}/SPPGraphicsO.cpp"
		)

target_include_directories(SPPGraphicsO 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )
			
target_link_libraries( SPPGraphicsO 
	PUBLIC
		SPPCore 
		SPPObject
		SPPMath
		SPPGraphics
		SPPSceneO
		)

install(TARGETS SPPGraphicsO DESTINATION "${SPP_ROOT}/INSTALL")



