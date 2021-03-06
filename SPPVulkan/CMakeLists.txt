cmake_minimum_required(VERSION 3.5)
project(SPPVulkan_API VERSION 1.0.0 LANGUAGES CXX)

add_definitions(-DUSE_VULKAN=1)

add_library(SPPVulkan SHARED "")

set_target_properties(SPPVulkan PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies(SPPVulkan 
	SPPEngine
	SPPGraphics
	SPPGraphicsO
	SPPSceneO
	SPPSDFO )

if(DEFINED ENV{VK_SDK_PATH})
	set(VK_SDK_PATH $ENV{VK_SDK_PATH})
else()
	message(FATAL_ERROR "Vulkan SDK env var not found VK_SDK_PATH")
endif()
			
target_sources(SPPVulkan
	PRIVATE			
		"${CMAKE_CURRENT_LIST_DIR}/SPPVulkan.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPVulkan.h"

		"${CMAKE_CURRENT_LIST_DIR}/VulkanShaders.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/VulkanShaders.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/VulkanDevice.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/VulkanDevice.h"

		"${CMAKE_CURRENT_LIST_DIR}/VulkanResources.h"
		"${CMAKE_CURRENT_LIST_DIR}/VulkanResources.cpp"
		
		"${CMAKE_CURRENT_LIST_DIR}/VulkanSwapChain.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/VulkanSwapChain.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/VulkanInitializers.hpp"
		
		"${CMAKE_CURRENT_LIST_DIR}/VulkanFrameBuffer.hpp"
		"${CMAKE_CURRENT_LIST_DIR}/VulkanFrameBuffer.cpp"
		
		"${CMAKE_CURRENT_LIST_DIR}/VulkanBuffer.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/VulkanBuffer.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/VulkanDebug.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/VulkanDebug.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/VulkanTexture.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/VulkanTexture.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/VulkanTools.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/VulkanTools.h"

		"${CMAKE_CURRENT_LIST_DIR}/VulkanRenderableMesh.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/VulkanRenderableMesh.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/VulkanRenderScene.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/VulkanRenderScene.h"

		"${CMAKE_CURRENT_LIST_DIR}/VulkanRenderableSDF.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/VulkanRenderableSDF.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/VulkanDebugDrawing.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/VulkanDebugDrawing.h"	

		"${CMAKE_CURRENT_LIST_DIR}/spirvreflect/spirv.h"
		"${CMAKE_CURRENT_LIST_DIR}/spirvreflect/spirv_reflect.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/spirvreflect/spirv_reflect.h"
		"${CMAKE_CURRENT_LIST_DIR}/spirvreflect/spirv_output_stream.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/spirvreflect/spirv_output_stream.h"

		"${THIRD_PARTY}/ktx/lib/texture.c"
		"${THIRD_PARTY}/ktx/lib/hashlist.c"
		"${THIRD_PARTY}/ktx/lib/hashtable.c"

		"${THIRD_PARTY}/ktx/lib/memstream.c"
		"${THIRD_PARTY}/ktx/lib/filestream.c"

		"${THIRD_PARTY}/ktx/lib/swap.c"
		"${THIRD_PARTY}/ktx/lib/checkheader.c"
		
		"${CMAKE_CURRENT_LIST_DIR}/imgui/imgui.cpp"		
		"${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_draw.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/imgui/imgui.h"	
		"${CMAKE_CURRENT_LIST_DIR}/imgui/imconfig.h"		
		"${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_internal.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/imgui/imstb_rectpack.h"
		"${CMAKE_CURRENT_LIST_DIR}/imgui/imstb_textedit.h"
		"${CMAKE_CURRENT_LIST_DIR}/imgui/imstb_truetype.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_impl_vulkan.cpp"		
		"${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_impl_vulkan.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_impl_win32.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_impl_win32.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_tables.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_widgets.cpp"
		)

target_include_directories(SPPVulkan 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )
		
		
		
target_include_directories(SPPVulkan 
	PRIVATE  			
		"${VK_SDK_PATH}/include"
		"${CMAKE_CURRENT_LIST_DIR}/imgui"
		"${THIRD_PARTY}/ktx/include"
		"${THIRD_PARTY}/ktx/other_include")
		
target_link_directories(SPPVulkan 
	PRIVATE
		"${VK_SDK_PATH}/lib" )		
		
		
target_link_libraries(SPPVulkan 
	PUBLIC
		SPPEngine
		SPPGraphics	
		SPPGraphicsO
		SPPSceneO
		SPPSDFO )
		
target_link_libraries( SPPVulkan 
	PRIVATE
		vulkan-1 )

install(TARGETS SPPVulkan DESTINATION "${SPP_ROOT}/INSTALL")



