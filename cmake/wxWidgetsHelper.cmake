function(TargetWxWidgetsSetup TargetName)
    
    target_compile_definitions(${TargetName}
    PRIVATE
        WXUSINGDLL=1)

    target_include_directories(${TargetName}
        PRIVATE
            "${THIRD_PARTY}/wxWidgets/include" )

    if(OS_WINDOWS)

        target_include_directories(${TargetName}
            PRIVATE
              "${THIRD_PARTY}/wxWidgets/include/msvc" )
              
        target_link_directories(${TargetName}
            PRIVATE
                "${THIRD_PARTY}/wxWidgets/lib/vc_x64_dll" )
                
    elseif(OS_MAC)

        target_include_directories(${TargetName}
            PRIVATE
              "${THIRD_PARTY}/wxWidgets/include/osx"
              "${THIRD_PARTY}/wxWidgets/lib/wx/include/osx_cocoa-unicode-3.1" )
              
        target_link_directories(${TargetName}
            PRIVATE
                "${THIRD_PARTY}/wxWidgets/lib" )
        
        target_link_libraries( ${TargetName}
            PRIVATE
                debug "wx_baseu-3.1d" optimized "wx_baseu-3.1"
                debug "wx_osx_cocoau_core-3.1d" optimized "wx_osx_cocoau_core-3.1"
                debug "wx_osx_cocoau_gl-3.1d" optimized "wx_osx_cocoau_gl-3.1"
                )
         
        target_compile_definitions(${TargetName}
            PRIVATE
                  __WXOSX_COCOA__
                  __WXMAC__
                  __WXOSX__ )

    endif()

  
  
  
endfunction()
