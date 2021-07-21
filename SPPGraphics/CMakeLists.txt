cmake_minimum_required(VERSION 3.5)
project(SPPGraphics_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPGraphics SHARED "")

set_target_properties(SPPGraphics PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPGraphics SPPEngine )

# will need define to not create empty dll export
target_compile_definitions( SPPGraphics
	PRIVATE
		SPP_GRAPHICSE_EXPORT=1)
				
target_sources( SPPGraphics
	PRIVATE
			
		"${CMAKE_CURRENT_LIST_DIR}/SPPGPUResources.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPGPUResources.h"
		"${CMAKE_CURRENT_LIST_DIR}/SPPGraphics.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPGraphics.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPMesh.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPMesh.h"
		
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
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPSceneGraph.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPSceneRendering.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPSceneRendering.h"

		"${CMAKE_CURRENT_LIST_DIR}/SPPTerrain.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPTerrain.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPTextures.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPTextures.h"
				
		"${CMAKE_CURRENT_LIST_DIR}/DX12/d3dx12.h"
		"${CMAKE_CURRENT_LIST_DIR}/DX12/DX12Buffers.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/DX12/DX12Buffers.h"
		"${CMAKE_CURRENT_LIST_DIR}/DX12/DX12Device.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/DX12/DX12Device.h"
		"${CMAKE_CURRENT_LIST_DIR}/DX12/DX12Shaders.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/DX12/DX12Shaders.h"
		"${CMAKE_CURRENT_LIST_DIR}/DX12/DX12Textures.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/DX12/DX12Textures.h"
		"${CMAKE_CURRENT_LIST_DIR}/DX12/DX12Utils.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/DX12/DX12Utils.h"

		)

target_include_directories(SPPEngine 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )
		
target_include_directories(SPPGraphics 
	PRIVATE  	
		"${CMAKE_CURRENT_LIST_DIR}/DX12"
		
		"${THIRD_PARTY}/stb" 		
		"${THIRD_PARTY}/assimp/include"
		"${THIRD_PARTY}/dxc/include"
		"${THIRD_PARTY}/eigen/include/eigen3"
		"${THIRD_PARTY}/fbtBlendHeaderOnly292"
		"${THIRD_PARTY}/metis-5.1.0/include" )
		
target_link_directories(SPPGraphics 
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/DX12/x64/DEBUG"
		
		"${THIRD_PARTY}/dxc/lib/x64"
		"${THIRD_PARTY}/assimp/lib/debug"
		"${THIRD_PARTY}/metis-5.1.0/lib/debug")		
		
		
target_link_libraries( SPPGraphics 
	PUBLIC
		SPPEngine )
		
target_link_libraries( SPPGraphics 
	PRIVATE
		d3d12
		dxgi
		d3dcompiler
		dxguid
		metis )

install(TARGETS SPPGraphics DESTINATION "${SPP_ROOT}/INSTALL")



