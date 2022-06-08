cmake_minimum_required(VERSION 3.5)
project(SPPGameEngine_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPGameEngine SHARED "")

set_target_properties( SPPGameEngine PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPGameEngine SPPCore SPPObject SPPMath SPPPhysX SPPSceneO SPPGraphicsO )


# will need define to not create empty dll export
target_compile_definitions(SPPGameEngine
	PRIVATE
		SPP_GAMEENGINE_EXPORT=1)
				
target_sources(SPPGameEngine
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPGameEngine.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPGameEngine.h"
		)
 	
target_link_libraries( SPPGameEngine 
	PUBLIC
		SPPCore 
		SPPObject
		SPPMath
		SPPPhysX
		SPPSceneO 
		SPPGraphicsO
		)

install(TARGETS SPPGameEngine DESTINATION "${SPP_ROOT}/INSTALL")



