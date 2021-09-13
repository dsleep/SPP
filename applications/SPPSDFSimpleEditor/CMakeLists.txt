cmake_minimum_required(VERSION 3.5)
project(SPPApplicationsGroup_API VERSION 1.0.0 LANGUAGES CXX)

if(OS_WINDOWS AND USE_GRAPHICS AND USE_DX12)

####################
	# basic dx 12 app with CEF
	####################
	add_executable( SimpleSDFEditor WIN32 "")
	add_dependencies( SimpleSDFEditor SPPSDFO SPPGraphics SPPDX12 SPPCEFUserInterface CEFProcess SPPSceneO SPPGraphicsO )			
	target_sources( SimpleSDFEditor 
		PRIVATE 
			"${CMAKE_CURRENT_LIST_DIR}/SimpleSDFEditorEntry.cpp"		
			"${SPP_ROOT}/SPPCEF/cefclient/resources/win/cefclient.rc" )		
	target_link_libraries( SimpleSDFEditor 
		PRIVATE
			SPPSDFO
			SPPGraphics
			SPPCEFUserInterface
			SPPSceneO
			SPPGraphicsO )
	set_target_properties( SimpleSDFEditor PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
	set_property(TARGET SimpleSDFEditor PROPERTY FOLDER "APPLICATIONS")
	#set_property(TARGET testDX12withCEFeditor PROPERTY VS_DPI_AWARE "PerMonitor"
	
endif()