cmake_minimum_required(VERSION 3.5)
project(SPPApplicationsGroup_API VERSION 1.0.0 LANGUAGES CXX)

if(OS_WINDOWS AND USE_GRAPHICS AND USE_DX12)

####################
	# basic dx 12 app with CEF
	####################
	add_executable( SPPAssetManagement WIN32 "")
	add_dependencies( SPPAssetManagement SPPSDFO SPPGraphics SPPDX12 SPPCEFUserInterface CEFProcess SPPSceneO SPPGraphicsO )			
	target_sources( SPPAssetManagement 
		PRIVATE 
			"${CMAKE_CURRENT_LIST_DIR}/SPPAssetManagement.cpp"		
			"${SPP_ROOT}/SPPCEF/cefclient/resources/win/cefclient.rc" )		
	target_link_libraries( SPPAssetManagement 
		PRIVATE
			SPPSDFO
			SPPGraphics
			SPPCEFUserInterface
			SPPSceneO
			SPPGraphicsO )
	set_target_properties( SPPAssetManagement PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
	set_property(TARGET SPPAssetManagement PROPERTY FOLDER "APPLICATIONS")
	#set_property(TARGET testDX12withCEFeditor PROPERTY VS_DPI_AWARE "PerMonitor"
	
endif()