cmake_minimum_required(VERSION 3.5)
project(SPPApplicationsGroup_API VERSION 1.0.0 LANGUAGES CXX)



###################################################
## SPPRemoteApplicationManager
###################################################
add_executable( SPPRemoteApplicationManager WIN32 "")
add_dependencies( SPPRemoteApplicationManager SPPCore SPPReflection )			
target_sources( SPPRemoteApplicationManager 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/SPPRemoteApplicationManager.cpp"					
		"${CMAKE_CURRENT_LIST_DIR}/sppbasicresource.rc" )		
target_link_libraries( SPPRemoteApplicationManager 
	PRIVATE
		SPPCore
		SPPReflection
	)
set_target_properties( SPPRemoteApplicationManager PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET SPPRemoteApplicationManager PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET SPPRemoteApplicationManager PROPERTY VS_DPI_AWARE "PerMonitor")

TargetWxWidgetsSetup( SPPRemoteApplicationManager )

install(TARGETS SPPRemoteApplicationManager DESTINATION "${SPP_ROOT}/INSTALL")

###################################################
## SPPRemoteApplicationController
###################################################

if(OS_WINDOWS)
    add_executable( SPPRemoteApplicationController WIN32 "")
else()
    add_executable( SPPRemoteApplicationController "")
endif()

add_dependencies( SPPRemoteApplicationController SPPCore SPPReflection )			
target_sources( SPPRemoteApplicationController 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/SPPRemoteApplicationController.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/sppbasicresource.rc"
	)		
target_link_libraries( SPPRemoteApplicationController 
	PRIVATE
		SPPCore
		SPPReflection
	)
set_target_properties( SPPRemoteApplicationController PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET SPPRemoteApplicationController PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET SPPRemoteApplicationController PROPERTY VS_DPI_AWARE "PerMonitor")

TargetWxWidgetsSetup( SPPRemoteApplicationController )

install(TARGETS SPPRemoteApplicationController DESTINATION "${SPP_ROOT}/INSTALL")

if(OS_WINDOWS)

install(DIRECTORY "${THIRD_PARTY}/wxWidgets/lib/vc_x64_dll/" #trailing / matters to flatten it
	DESTINATION "${SPP_ROOT}/INSTALL"
	FILES_MATCHING 
		PATTERN "*.dll"
		PATTERN mswu EXCLUDE 
		PATTERN mswud EXCLUDE)

endif()