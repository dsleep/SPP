cmake_minimum_required(VERSION 3.5)
project(SPPMesh_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPMesh SHARED "")

set_target_properties(SPPMesh PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPMesh SPPMath  )


# will need define to not create empty dll export
target_compile_definitions( SPPMesh
	PRIVATE
		SPP_MESH_EXPORT=1)
				
target_sources( SPPMesh
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPMesh.h"
		"${CMAKE_CURRENT_LIST_DIR}/SPPMesh.cpp" 		
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPSkinnedMesh.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPSkinnedMesh.h"

		"${CMAKE_CURRENT_LIST_DIR}/SPPBlenderFile.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPBlenderFile.h"
				
		"${CMAKE_CURRENT_LIST_DIR}/SPPAssetImporterFile.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPAssetImporterFile.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPMeshlets.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPMeshlets.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPMeshSimplifying.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPMeshSimplifying.h"
		)

target_include_directories(SPPMesh 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}"
		)				
			
target_link_libraries( SPPMesh  
	PUBLIC
		SPPMath 
		)
		
target_include_directories(SPPMesh 
	PRIVATE  					
		"${THIRD_PARTY}/assimp/include"
		"${THIRD_PARTY}/eigen/include/eigen3"
		"${THIRD_PARTY}/fbtBlendHeaderOnly/301"
		"${THIRD_PARTY}/metis/include" )
		
target_link_directories(SPPMesh 
	PRIVATE
		"${THIRD_PARTY}/assimp/lib"		
		debug "${THIRD_PARTY}/metis/lib/Debug" optimized "${THIRD_PARTY}/metis/lib/Release" )		
		
		
target_link_libraries( SPPMesh 
	PRIVATE
		metis )
		
install(TARGETS SPPMesh DESTINATION "${SPP_ROOT}/INSTALL")



