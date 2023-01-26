cmake_minimum_required(VERSION 3.5)
project(SPPApplications_API VERSION 1.0.0 LANGUAGES CXX)

#message( STATUS "Adding Application: SPPMesher" )
#add_subdirectory("SPPMesher")

install(FILES "${SPP_ROOT}/Binaries/config.txt" DESTINATION "${SPP_ROOT}/INSTALL")

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

install(TARGETS simpleconnectioncoordinator DESTINATION "${SPP_ROOT}/INSTALL")

##########
add_executable( SPPApplicationMonitor "")
add_dependencies( SPPApplicationMonitor SPPCore )			
target_sources( SPPApplicationMonitor 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/SPPApplicationMonitor.cpp" )		
target_link_libraries( SPPApplicationMonitor SPPCore )
set_target_properties( SPPApplicationMonitor PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET SPPApplicationMonitor PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET SPPApplicationMonitor PROPERTY VS_DPI_AWARE "PerMonitor")

install(TARGETS SPPApplicationMonitor DESTINATION "${SPP_ROOT}/INSTALL")

if(OS_LINUX)
return()
endif()

####################
# remoteviewer
####################

set(SPP_BTE_PROJECT "")

if(OS_WINDOWS)
    set(SPP_BTE_PROJECT "SPPWinRTBTE")
endif()
if(OS_MAC)
    set(SPP_BTE_PROJECT "SPPMacBT")
endif()

add_executable( remoteviewer "")
add_dependencies( remoteviewer SPPNetworking SPPEngine SPPVideo) # ${SPP_BTE_PROJECT} )
target_sources( remoteviewer
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/remoteviewer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/sppbasicresource.rc" )
target_link_libraries( remoteviewer
    PRIVATE
        SPPNetworking
        SPPEngine
        SPPVideo)
        #${SPP_BTE_PROJECT} )
        
set_target_properties( remoteviewer PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET remoteviewer PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET remoteviewer PROPERTY VS_DPI_AWARE "PerMonitor")

TargetWxWidgetsSetup( remoteviewer )

if(OS_MAC)
   target_link_libraries( remoteviewer
        PRIVATE
            "-framework OpenGL" )
endif()

install(TARGETS remoteviewer DESTINATION "${SPP_ROOT}/INSTALL")

####################
# END remoteviewer
####################


if(OS_WINDOWS)

message( STATUS "Adding Application: SPPSDFSimpleEditor" )
add_subdirectory("SPPSDFSimpleEditor")

message( STATUS "Adding Application: SPPSDFCreator" )
add_subdirectory("SPPSDFCreator")

message( STATUS "Adding Application: SPPAssetManagement" )
add_subdirectory("SPPAssetManagement")

message( STATUS "Adding Application: SPPRemoteApplicationwxWidgets" )
add_subdirectory("SPPRemoteApplicationwxWidgets")

##########
add_executable( applicationhost WIN32 "")
add_dependencies( applicationhost SPPNetworking SPPEngine SPPVideo SPPCapture)			
target_sources( applicationhost 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/applicationhost.cpp"
        "${SPP_ROOT}/SPPEngine/Win32/sppbasicresource.rc" )		
target_link_libraries( applicationhost SPPNetworking SPPEngine SPPVideo SPPCapture )
set_target_properties( applicationhost PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET applicationhost PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET applicationhost PROPERTY VS_DPI_AWARE "PerMonitor")


#if(OS_WINDOWS)
#    set_target_properties(applicationhost PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/DELAYLOAD:SPPVideo.dll" )
#    set_target_properties(applicationhost PROPERTIES LINK_FLAGS_RELEASE "/DELAYLOAD:SPPVideo.dll" )
#    set_target_properties(applicationhost PROPERTIES LINK_FLAGS_DEBUG "/DELAYLOAD:SPPVideo.dll" )
#endif() Delayimp

install(TARGETS applicationhost DESTINATION "${SPP_ROOT}/INSTALL")

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

if(OS_MAC)
	message( STATUS "Adding Application: SPPRemoteApplicationwxWidgets" )
	add_subdirectory("SPPRemoteApplicationwxWidgets")
endif()



    
