cmake_minimum_required(VERSION 3.5)
project(SPPApplicationsGroup_API VERSION 1.0.0 LANGUAGES CXX)


if(OS_WINDOWS)

###################################################
## SPPRemoteApplicationManager
###################################################
# add_executable( SPPRemoteApplicationManager WIN32 "")
# add_dependencies( SPPRemoteApplicationManager SPPCore SPPReflection )
# target_sources( SPPRemoteApplicationManager 
	# PRIVATE 
		# "${CMAKE_CURRENT_LIST_DIR}/SPPRemoteApplicationManager.cpp"					
		# "${SPP_ROOT}/SPPEngine/Win32/sppbasicresource.rc" )		
# target_link_libraries( SPPRemoteApplicationManager 
	# PRIVATE
		# SPPCore
		# SPPReflection
	# )
# set_target_properties( SPPRemoteApplicationManager PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
# set_property(TARGET SPPRemoteApplicationManager PROPERTY FOLDER "APPLICATIONS")
# set_property(TARGET SPPRemoteApplicationManager PROPERTY VS_DPI_AWARE "PerMonitor")

# TargetWxWidgetsSetup( SPPRemoteApplicationManager )

# install(TARGETS SPPRemoteApplicationManager DESTINATION "${SPP_ROOT}/INSTALL")

###################################################
## SPPRemoteDesktop
###################################################
add_executable( SPPRemoteDesktop WIN32 "")
add_dependencies( SPPRemoteDesktop SPPCore SPPNetworking SPPReflection SPPCEFUserInterface )
target_sources( SPPRemoteDesktop 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/SPPRemoteDesktop.cpp"					
		"${SPP_ROOT}/SPPCEF/cefclient/resources/win/cefclient.rc" )		
target_link_libraries( SPPRemoteDesktop 
	PRIVATE
		SPPCore
        SPPNetworking
		SPPReflection
        SPPCEFUserInterface
	)
set_target_properties( SPPRemoteDesktop PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET SPPRemoteDesktop PROPERTY FOLDER "APPLICATIONS")
#set_property(TARGET SPPRemoteDesktop PROPERTY VS_DPI_AWARE "PerMonitor")

TargetWxWidgetsSetup( SPPRemoteDesktop )

install(TARGETS SPPRemoteDesktop DESTINATION "${SPP_ROOT}/INSTALL")


endif()

# ###################################################
# ## SPPRemoteApplicationController
# ###################################################

# if(OS_WINDOWS)
    # add_executable( SPPRemoteApplicationController WIN32 "")
# else()
    # add_executable( SPPRemoteApplicationController "")
# endif()

# add_dependencies( SPPRemoteApplicationController SPPCore SPPReflection remoteviewer )
# target_sources( SPPRemoteApplicationController 
	# PRIVATE 
		# "${CMAKE_CURRENT_LIST_DIR}/SPPRemoteApplicationController.cpp"
		# "${SPP_ROOT}/SPPEngine/Win32/sppbasicresource.rc"
	# )		
# target_link_libraries( SPPRemoteApplicationController 
	# PRIVATE
		# SPPCore
		# SPPReflection
	# )
# set_target_properties( SPPRemoteApplicationController PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
# set_property(TARGET SPPRemoteApplicationController PROPERTY FOLDER "APPLICATIONS")
# set_property(TARGET SPPRemoteApplicationController PROPERTY VS_DPI_AWARE "PerMonitor")

# TargetWxWidgetsSetup( SPPRemoteApplicationController )

# if(OS_MAC)

    # set(APP_NAME "SPPRemoteApplicationController" )
    # set(APP_BUNDLE_IDENTIFIER "tech.sleepingrobot.SPPRemoteApplicationController" )

    # set_target_properties(SPPRemoteApplicationController PROPERTIES
        # MACOSX_BUNDLE TRUE
        # MACOSX_BUNDLE_INFO_PLIST "${SPP_ROOT}/Misc/Mac/Info.plist.in"
        # MACOSX_BUNDLE_BUNDLE_NAME ${APP_NAME}
        
        # EXECUTABLE_NAME ${APP_NAME}
        
        # MACOSX_BUNDLE_EXECUTABLE_NAME ${APP_NAME}
        # MACOSX_BUNDLE_INFO_STRING ${APP_BUNDLE_IDENTIFIER}
        # MACOSX_BUNDLE_GUI_IDENTIFIER ${APP_BUNDLE_IDENTIFIER}
        # MACOSX_BUNDLE_BUNDLE_NAME ${APP_BUNDLE_IDENTIFIER}
        # MACOSX_DEVELOPMENT_LANGUAGE "en"
        # MACOSX_BUNDLE_ICON_FILE ""
        # MACOSX_BUNDLE_LONG_VERSION_STRING "1.0"
        # MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        # MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        # MACOSX_BUNDLE_COPYRIGHT "Copyright ©2022 Sleeping Robot LLC. All rights reserved."
        
        # RESOURCE "${SPP_ROOT}/Misc/Mac/MainAppWindow.xib"
# #        XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
    # )
    # target_sources( SPPRemoteApplicationController
        # PRIVATE
            # "${SPP_ROOT}/Misc/Mac/MainAppWindow.xib"  )
            
    # add_custom_command(TARGET SPPRemoteApplicationController POST_BUILD
        # COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:remoteviewer> ${SPP_ROOT}/Binaries/SPPRemoteApplicationController.app/Contents/MacOS
        # COMMENT "Copied RemoteViewer Into RAC"
        # )
        
    # add_custom_command(TARGET SPPRemoteApplicationController POST_BUILD
        # COMMAND ${CMAKE_COMMAND} -E copy ${SPP_ROOT}/Binaries/config.txt ${SPP_ROOT}/Binaries/SPPRemoteApplicationController.app/Contents/Resources
        # COMMENT "Copied RemoteViewer Into RAC"
        # )
        
    # install(TARGETS SPPRemoteApplicationController DESTINATION "${SPP_ROOT}/INSTALL")
    # install(FILES "${SPP_ROOT}/Binaries/config.txt" DESTINATION "${SPP_ROOT}/INSTALL/SPPRemoteApplicationController.app/Contents/Resources")
            
    # install(TARGETS remoteviewer DESTINATION "${SPP_ROOT}/INSTALL/SPPRemoteApplicationController.app/Contents/MacOS")

    # install(CODE "set(SPP_ROOT \"${SPP_ROOT}\")")
    # install(CODE "set(THIRD_PARTY \"${THIRD_PARTY}\")")
        
    # install(CODE [[
        # list(APPEND LIB_DIRS "${THIRD_PARTY}")
        # ]])
    # install(CODE [[
        # list(APPEND LIB_DIRS "${THIRD_PARTY}/wxWidgets/lib")
        # ]])
    # install(CODE [[
        # list(APPEND LIB_DIRS "${THIRD_PARTY}/rttr/lib")
        # ]])
    # install(CODE [[
        # list(APPEND LIB_DIRS "${THIRD_PARTY}/jsoncpp/lib")
        # ]])
    # install(CODE [[
        # list(APPEND LIB_DIRS "${THIRD_PARTY}/cryptopp/lib")
        # ]])
    # install(CODE [[
        # list(APPEND LIB_DIRS "${THIRD_PARTY}/libjuice/lib")
        # ]])
            
    # install(CODE [[
        # include(BundleUtilities)
        # fixup_bundle("${CMAKE_INSTALL_PREFIX}/SPPRemoteApplicationController.app" "" "${LIB_DIRS}")
    # ]] COMPONENT Runtime)
    
# if(false)
    # # Transfer the value of ${MY_DEPENDENCY_PATHS} into the install script
  
        
    # install(CODE [[
  # file(GET_RUNTIME_DEPENDENCIES
    # EXECUTABLES "${SPP_ROOT}/Binaries/remoteviewerd"
    # RESOLVED_DEPENDENCIES_VAR _r_deps
    # UNRESOLVED_DEPENDENCIES_VAR _u_deps
  # )
  # foreach(_file ${_r_deps})
      # message(WARNING "TTTT ${_file}")
    # file(INSTALL
      # DESTINATION "${CMAKE_INSTALL_PREFIX}/lib"
      # TYPE SHARED_LIBRARY
      # FOLLOW_SYMLINK_CHAIN
      # FILES "${_file}"
    # )
  # endforeach()
  # list(LENGTH _u_deps _u_length)
  # if("${_u_length}" GREATER 0)
    # message(WARNING "Unresolved dependencies detected!")
  # endif()
# ]])

# endif()

# endif()




# if(OS_WINDOWS)

# install(TARGETS SPPRemoteApplicationController DESTINATION "${SPP_ROOT}/INSTALL")

# install(DIRECTORY "${THIRD_PARTY}/wxWidgets/lib/vc_x64_dll/" #trailing / matters to flatten it
	# DESTINATION "${SPP_ROOT}/INSTALL"
	# FILES_MATCHING 
		# PATTERN "*.dll"
		# PATTERN mswu EXCLUDE 
		# PATTERN mswud EXCLUDE)

# endif()

