cmake_minimum_required(VERSION 3.5)
project(SPPTests_API VERSION 1.0.0 LANGUAGES CXX)

##########
add_executable( objectGeneralTests "")
add_dependencies( objectGeneralTests SPPObject)			
target_sources( objectGeneralTests 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/objectGeneralTests.cpp" )		
target_link_libraries( objectGeneralTests SPPObject )
set_target_properties( objectGeneralTests PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET objectGeneralTests PROPERTY FOLDER "TESTS")
set_property(TARGET objectGeneralTests PROPERTY VS_DPI_AWARE "PerMonitor")

if(OS_WINDOWS)

##########
add_executable( bluetoothTest "")
add_dependencies( bluetoothTest SPPNetworking)			
target_sources( bluetoothTest 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/bluetoothTest.cpp" )		
target_link_libraries( bluetoothTest SPPNetworking )
set_target_properties( bluetoothTest PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET bluetoothTest PROPERTY FOLDER "TESTS")
set_property(TARGET bluetoothTest PROPERTY VS_DPI_AWARE "PerMonitor")


##########
add_executable( networkTest "")
add_dependencies( networkTest SPPNetworking)			
target_sources( networkTest 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/networkTest.cpp" )		
target_link_libraries( networkTest SPPNetworking )
set_target_properties( networkTest PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET networkTest PROPERTY FOLDER "TESTS")
set_property(TARGET networkTest PROPERTY VS_DPI_AWARE "PerMonitor")

##########
add_executable( encryptionTest "")
add_dependencies( encryptionTest SPPCrypto )			
target_sources( encryptionTest 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/encryptionTest.cpp" )		
target_link_libraries( encryptionTest SPPCrypto )
set_target_properties( encryptionTest PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET encryptionTest PROPERTY FOLDER "TESTS")
set_property(TARGET encryptionTest PROPERTY VS_DPI_AWARE "PerMonitor")


##########
add_executable( IPCMemTest "")
add_dependencies( IPCMemTest SPPCore )			
target_sources( IPCMemTest 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/IPCMemTest.cpp" )		
target_link_libraries( IPCMemTest SPPCore )
set_target_properties( IPCMemTest PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET IPCMemTest PROPERTY FOLDER "TESTS")
set_property(TARGET IPCMemTest PROPERTY VS_DPI_AWARE "PerMonitor")

##########
add_executable( appTransferTest "")
add_dependencies( appTransferTest SPPCrypto SPPNetworking )			
target_sources( appTransferTest 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/appTransferTest.cpp" )		
target_link_libraries( appTransferTest SPPNetworking SPPCrypto )
set_target_properties( appTransferTest PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET appTransferTest PROPERTY FOLDER "TESTS")
set_property(TARGET appTransferTest PROPERTY VS_DPI_AWARE "PerMonitor")


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
	set_property(TARGET testOpenGLApp PROPERTY FOLDER "TESTS")
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
	set_property(TARGET testDX12App PROPERTY FOLDER "TESTS")
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
	set_property(TARGET testDX12withCEFeditor PROPERTY FOLDER "TESTS")
	#set_property(TARGET testDX12withCEFeditor PROPERTY VS_DPI_AWARE "PerMonitor")
endif()

endif()