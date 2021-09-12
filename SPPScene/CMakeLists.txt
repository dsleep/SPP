cmake_minimum_required(VERSION 3.5)
project(SPPScene_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPScene SHARED "")

set_target_properties(SPPScene PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPScene SPPCore SPPObject SPPMath )


# will need define to not create empty dll export
target_compile_definitions(SPPScene
	PRIVATE
		SPP_SCENE_EXPORT=1)
				
target_sources(SPPScene
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPScene.h"
		"${CMAKE_CURRENT_LIST_DIR}/SPPScene.cpp"
		)

target_include_directories(SPPScene 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )
			
target_link_libraries( SPPScene 
	PUBLIC
		SPPCore 
		SPPObject
		SPPMath
		)

install(TARGETS SPPEngine DESTINATION "${SPP_ROOT}/INSTALL")



