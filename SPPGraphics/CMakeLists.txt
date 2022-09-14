cmake_minimum_required(VERSION 3.5)
project(SPPGraphics_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPGraphics SHARED "")

set_target_properties(SPPGraphics PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPGraphics SPPEngine SPPMesh )

# will need define to not create empty dll export
target_compile_definitions( SPPGraphics
	PRIVATE
		SPP_GRAPHICSE_EXPORT=1
		KHRONOS_STATIC=1)
		
target_sources( SPPGraphics
	PRIVATE
			
		"${CMAKE_CURRENT_LIST_DIR}/SPPGPUResources.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPGPUResources.h"
		"${CMAKE_CURRENT_LIST_DIR}/SPPGraphics.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPGraphics.h"
						
		"${CMAKE_CURRENT_LIST_DIR}/SPPSceneGraph.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPSceneRendering.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPSceneRendering.h"

		"${CMAKE_CURRENT_LIST_DIR}/SPPTerrain.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPTerrain.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPTextures.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPTextures.h"				

		)

target_include_directories( SPPGraphics 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )
		
		
target_link_libraries( SPPGraphics 
	PUBLIC
		SPPEngine
		SPPMesh	
	PRIVATE
		debug ktxd optimized ktx )
		
target_link_directories( SPPGraphics 
	PRIVATE
		"${THIRD_PARTY}/ktx/lib" )		
		

target_include_directories( SPPGraphics 
	PRIVATE  			
		"${THIRD_PARTY}/stb"
		"${THIRD_PARTY}/ktx/include" )

install(TARGETS SPPGraphics DESTINATION "${SPP_ROOT}/INSTALL")



