cmake_minimum_required(VERSION 3.5)
project(SPPSceneO_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPSceneO SHARED "")

set_target_properties(SPPSceneO PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPSceneO SPPCore SPPObject SPPMath )


# will need define to not create empty dll export
target_compile_definitions(SPPSceneO
	PRIVATE
		SPP_SCENEO_EXPORT=1)
				
target_sources(SPPSceneO
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPSceneO.h"
		"${CMAKE_CURRENT_LIST_DIR}/SPPSceneO.cpp"
		)

target_include_directories(SPPSceneO 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )
			
target_link_libraries( SPPSceneO 
	PUBLIC
		SPPCore 
		SPPObject
		SPPMath
		)

install(TARGETS SPPSceneO DESTINATION "${SPP_ROOT}/INSTALL")



