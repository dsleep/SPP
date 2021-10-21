cmake_minimum_required(VERSION 3.5)
project(SPPApplicationsGroup_API VERSION 1.0.0 LANGUAGES CXX)

###################################################
## SPPRemoteApplicationManager
###################################################
add_executable( SPPRemoteApplicationManager WIN32 "")
add_dependencies( SPPRemoteApplicationManager SPPCore )			
target_sources( SPPRemoteApplicationManager 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/SPPRemoteApplicationManager.cpp"					
		"${CMAKE_CURRENT_LIST_DIR}/sppbasicresource.rc" )		
target_link_libraries( SPPRemoteApplicationManager 
	PRIVATE
		SPPCore
	)
set_target_properties( SPPRemoteApplicationManager PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET SPPRemoteApplicationManager PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET SPPRemoteApplicationManager PROPERTY VS_DPI_AWARE "PerMonitor")

target_compile_definitions(SPPRemoteApplicationManager
	PRIVATE
		WXUSINGDLL=1)
				

target_include_directories(SPPRemoteApplicationManager 
	PRIVATE  	
		"${THIRD_PARTY}/wxWidgets/include"
		"${THIRD_PARTY}/wxWidgets/include/msvc" )
		
target_link_directories(SPPRemoteApplicationManager 
	PRIVATE
		"${THIRD_PARTY}/wxWidgets/lib/vc_x64_dll" )		
		
		
###################################################
## SPPRemoteApplicationController
###################################################

add_executable( SPPRemoteApplicationController WIN32 "")
add_dependencies( SPPRemoteApplicationController SPPCore )			
target_sources( SPPRemoteApplicationController 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/SPPRemoteApplicationController.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/sppbasicresource.rc"
	)		
target_link_libraries( SPPRemoteApplicationController 
	PRIVATE
		SPPCore
	)
set_target_properties( SPPRemoteApplicationController PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET SPPRemoteApplicationController PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET SPPRemoteApplicationController PROPERTY VS_DPI_AWARE "PerMonitor")

target_compile_definitions(SPPRemoteApplicationController
	PRIVATE
		WXUSINGDLL=1)
				

target_include_directories(SPPRemoteApplicationController 
	PRIVATE  	
		"${THIRD_PARTY}/wxWidgets/include"
		"${THIRD_PARTY}/wxWidgets/include/msvc" )
		
target_link_directories(SPPRemoteApplicationController 
	PRIVATE
		"${THIRD_PARTY}/wxWidgets/lib/vc_x64_dll" )		
		