cmake_minimum_required(VERSION 3.5)
project(SPPApplications_API VERSION 1.0.0 LANGUAGES CXX)

##########
add_executable( simpleconnectioncoordinator "")
add_dependencies( simpleconnectioncoordinator SPPNetworking )			
target_sources( simpleconnectioncoordinator 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/simpleconnectioncoordinator.cpp" )		
target_link_libraries( simpleconnectioncoordinator SPPNetworking )
set_target_properties( simpleconnectioncoordinator PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET simpleconnectioncoordinator PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET simpleconnectioncoordinator PROPERTY VS_DPI_AWARE "PerMonitor")

if(OS_WINDOWS)

##########
add_executable( networkTest "")
add_dependencies( networkTest SPPNetworking)			
target_sources( networkTest 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/networkTest.cpp" )		
target_link_libraries( networkTest SPPNetworking )
set_target_properties( networkTest PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET networkTest PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET networkTest PROPERTY VS_DPI_AWARE "PerMonitor")

##########
add_executable( encryptionTest "")
add_dependencies( encryptionTest SPPCrypto )			
target_sources( encryptionTest 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/encryptionTest.cpp" )		
target_link_libraries( encryptionTest SPPCrypto )
set_target_properties( encryptionTest PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET encryptionTest PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET encryptionTest PROPERTY VS_DPI_AWARE "PerMonitor")

##########
add_executable( applicationhost "")
add_dependencies( applicationhost SPPNetworking SPPVideo_CUDA SPPCapture)			
target_sources( applicationhost 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/applicationhost.cpp" )		
target_link_libraries( applicationhost SPPNetworking SPPVideo_CUDA SPPCapture )
set_target_properties( applicationhost PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET applicationhost PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET applicationhost PROPERTY VS_DPI_AWARE "PerMonitor")

set_target_properties(applicationhost PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/DELAYLOAD:SPPVideo_CUDA.dll" )
set_target_properties(applicationhost PROPERTIES LINK_FLAGS_RELEASE "/DELAYLOAD:SPPVideo_CUDA.dll" )
set_target_properties(applicationhost PROPERTIES LINK_FLAGS_DEBUG "/DELAYLOAD:SPPVideo_CUDAd.dll" )

####################
# remoteviewer
####################
add_executable( remoteviewer "")
add_dependencies( remoteviewer SPPNetworking SPPEngine SPPVideo )			
target_sources( remoteviewer 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/remoteviewer.cpp" )		
target_link_libraries( remoteviewer 
	PRIVATE
		SPPNetworking 
		SPPEngine 
		SPPVideo )
set_target_properties( remoteviewer PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET remoteviewer PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET remoteviewer PROPERTY VS_DPI_AWARE "PerMonitor")
	
set_target_properties(remoteviewer PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/DELAYLOAD:SPPVideo.dll" )
set_target_properties(remoteviewer PROPERTIES LINK_FLAGS_RELEASE "/DELAYLOAD:SPPVideo.dll" )
set_target_properties(remoteviewer PROPERTIES LINK_FLAGS_DEBUG "/DELAYLOAD:SPPVideod.dll" )


##########
add_executable( appTransferTest "")
add_dependencies( appTransferTest SPPCrypto SPPNetworking )			
target_sources( appTransferTest 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/appTransferTest.cpp" )		
target_link_libraries( appTransferTest SPPNetworking SPPCrypto )
set_target_properties( appTransferTest PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET appTransferTest PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET appTransferTest PROPERTY VS_DPI_AWARE "PerMonitor")


####################
# remoteviewer
####################
add_executable( windowsEventApp WIN32 "")
add_dependencies( windowsEventApp SPPNetworking SPPEngine SPPVideo )			
target_sources( windowsEventApp 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/windowsEventApp.cpp" )		
target_link_libraries( windowsEventApp 
	PRIVATE
		SPPEngine
		SPPCapture )
set_target_properties( windowsEventApp PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET windowsEventApp PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET windowsEventApp PROPERTY VS_DPI_AWARE "PerMonitor")


if(USE_GRAPHICS AND USE_DX12)
	####################
	# basic opengl app
	####################
	add_executable( testOpenGLApp WIN32 "")
	add_dependencies( testOpenGLApp SPPGraphics SPPOpenGL )			
	target_sources( testOpenGLApp 
		PRIVATE 
			"${CMAKE_CURRENT_LIST_DIR}/testOpenGLApp.cpp" )		
	target_link_libraries( testOpenGLApp 
		PRIVATE
			SPPGraphics )
	set_target_properties( testOpenGLApp PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
	set_property(TARGET testOpenGLApp PROPERTY FOLDER "APPLICATIONS")
	set_property(TARGET testOpenGLApp PROPERTY VS_DPI_AWARE "PerMonitor")
	
	####################
	# basic dx 12 app
	####################
	add_executable( testDX12App WIN32 "")
	add_dependencies( testDX12App SPPGraphics SPPDX12 )			
	target_sources( testDX12App 
		PRIVATE 
			"${CMAKE_CURRENT_LIST_DIR}/testDX12App.cpp" )		
	target_link_libraries( testDX12App 
		PRIVATE
			SPPGraphics )
	set_target_properties( testDX12App PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
	set_property(TARGET testDX12App PROPERTY FOLDER "APPLICATIONS")
	set_property(TARGET testDX12App PROPERTY VS_DPI_AWARE "PerMonitor")

	####################
	# basic dx 12 app with CEF
	####################
	add_executable( testDX12withCEFeditor WIN32 "")
	add_dependencies( testDX12withCEFeditor SPPGraphics SPPDX12 SPPCEFUserInterface CEFProcess )			
	target_sources( testDX12withCEFeditor 
		PRIVATE 
			"${CMAKE_CURRENT_LIST_DIR}/testDX12withCEFeditor.cpp"		
			"${SPP_ROOT}/SPPCEF/cefclient/resources/win/cefclient.rc" )		
	target_link_libraries( testDX12withCEFeditor 
		PRIVATE
			SPPGraphics
			SPPCEFUserInterface )
	set_target_properties( testDX12withCEFeditor PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
	set_property(TARGET testDX12withCEFeditor PROPERTY FOLDER "APPLICATIONS")
	#set_property(TARGET testDX12withCEFeditor PROPERTY VS_DPI_AWARE "PerMonitor")
endif()

endif()