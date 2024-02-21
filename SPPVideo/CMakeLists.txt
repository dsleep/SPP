cmake_minimum_required(VERSION 3.5)
project(SPPVideo_API VERSION 1.0.0 LANGUAGES CXX)

#ITS ugly but it is what it is CUDA copy below
add_library(SPPVideo SHARED "")
set_target_properties(SPPVideo PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
add_dependencies( SPPVideo SPPCore )

# will need define to not create empty dll export
target_compile_definitions(SPPVideo
	PRIVATE
		SPP_VIDEO_EXPORT=1)
				
target_sources(SPPVideo
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPVideo.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPVideo.h"				
		)

target_include_directories(SPPVideo 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )

target_include_directories(SPPVideo 
	PRIVATE  	
		"${THIRD_PARTY}/ffmpeg/include" )
		
target_link_directories(SPPVideo 
	PRIVATE		
		"${THIRD_PARTY}/ffmpeg/lib" )		
			
target_link_libraries( SPPVideo 
	PUBLIC
		SPPCore )		

target_link_libraries( SPPVideo 
	PRIVATE
		avcodec
		avformat
		swresample
		avutil
		swscale)

set(DELAY_LOAD_LIST
	"avcodec-59"
	"avdevice-59"
	"avformat-59"
	"avutil-57"
	"swresample-4"
	"swscale-6" )
		
set(DELAY_LOAD_STR "")

foreach(curDelayLoad ${DELAY_LOAD_LIST})	
    set(DELAY_LOAD_STR "/DELAYLOAD:${curDelayLoad}.dll ${DELAY_LOAD_STR}")
endforeach()    

if(OS_WINDOWS)
    set_target_properties(SPPVideo PROPERTIES LINK_FLAGS_RELWITHDEBINFO "${DELAY_LOAD_STR}" )
    set_target_properties(SPPVideo PROPERTIES LINK_FLAGS_RELEASE "${DELAY_LOAD_STR}" )
    set_target_properties(SPPVideo PROPERTIES LINK_FLAGS_DEBUG "${DELAY_LOAD_STR}" )
endif() 
  
if(OS_MAC)

target_link_libraries( SPPVideo
    PRIVATE
        z
        BZ2
        "-framework CoreFoundation"
        "-framework VideoDecodeAcceleration"
        "-framework CoreGraphics"
        "-framework AVFoundation"
        "-framework CoreVideo" )

endif()

install(TARGETS SPPVideo DESTINATION "${SPP_ROOT}/INSTALL")

