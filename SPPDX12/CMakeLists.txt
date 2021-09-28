cmake_minimum_required(VERSION 3.5)
project(SPPDX12_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPDX12 SHARED "")

set_target_properties(SPPDX12 PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPDX12 SPPEngine )
			
target_sources( SPPDX12
	PRIVATE			
		"${CMAKE_CURRENT_LIST_DIR}/d3dx12.h"
		"${CMAKE_CURRENT_LIST_DIR}/DX12Buffers.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/DX12Buffers.h"
		"${CMAKE_CURRENT_LIST_DIR}/DX12Device.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/DX12Device.h"
		"${CMAKE_CURRENT_LIST_DIR}/DX12Shaders.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/DX12Shaders.h"
		"${CMAKE_CURRENT_LIST_DIR}/DX12Textures.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/DX12Textures.h"
		"${CMAKE_CURRENT_LIST_DIR}/DX12Utils.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/DX12Utils.h"
		
		
		"${CMAKE_CURRENT_LIST_DIR}/DX12RenderScene.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/DX12RenderScene.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/DX12RenderableMesh.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/DX12RenderableMesh.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/DX12RenderableSDF.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/DX12RenderableSDF.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/AMD/D3D12MemAlloc.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/AMD/D3D12MemAlloc.h"
		)

target_include_directories(SPPDX12 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )
		
target_include_directories(SPPDX12 
	PRIVATE  			
		"${THIRD_PARTY}/dxc/include"
		"${THIRD_PARTY}/AMD" )
		
target_link_directories(SPPDX12 
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/DX12/x64/DEBUG"		
		"${THIRD_PARTY}/dxc/lib/x64")		
		
		
target_link_libraries( SPPDX12 
	PUBLIC
		SPPEngine
		SPPGraphics	)
		
target_link_libraries( SPPDX12 
	PRIVATE
		d3d12
		dxgi
		d3dcompiler
		dxguid )

install(TARGETS SPPDX12 DESTINATION "${SPP_ROOT}/INSTALL")



