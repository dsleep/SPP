cmake_minimum_required(VERSION 3.5)
project(SPPApplications_API VERSION 1.0.0 LANGUAGES CXX)

message( STATUS "Adding Application: SPPSDFSimpleEditor" )
add_subdirectory("SPPSDFSimpleEditor")

message( STATUS "Adding Application: SPPSDFCreator" )
add_subdirectory("SPPSDFCreator")

message( STATUS "Adding Application: SPPAssetManagement" )
add_subdirectory("SPPAssetManagement")

message( STATUS "Adding Application: SPPRemoteApplicationwxWidgets" )
add_subdirectory("SPPRemoteApplicationwxWidgets")

#message( STATUS "Adding Application: SPPMesher" )
#add_subdirectory("SPPMesher")

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
# windows only event app test
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

endif()

####################
# remoteviewer
####################
if(OS_WINDOWS)

    add_executable( remoteviewer "")
    add_dependencies( remoteviewer SPPNetworking SPPEngine SPPVideo SPPWinRTBTE )
    target_sources( remoteviewer
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/remoteviewer.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/sppbasicresource.rc" )
    target_link_libraries( remoteviewer
        PRIVATE
            SPPNetworking
            SPPEngine
            SPPVideo 
            SPPWinRTBTE )        
endif()

set_target_properties( remoteviewer PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET remoteviewer PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET remoteviewer PROPERTY VS_DPI_AWARE "PerMonitor")

if(OS_WINDOWS)
    set_target_properties(remoteviewer PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/DELAYLOAD:SPPVideo.dll" )
    set_target_properties(remoteviewer PROPERTIES LINK_FLAGS_RELEASE "/DELAYLOAD:SPPVideo.dll" )
    set_target_properties(remoteviewer PROPERTIES LINK_FLAGS_DEBUG "/DELAYLOAD:SPPVideod.dll" )
endif()

if(OS_MAC)
   target_link_libraries( remoteviewer
        PRIVATE
            "-framework OpenGL" )
endif()

TargetWxWidgetsSetup( remoteviewer )
