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
		"${THIRD_PARTY}/libav/include" )
		
target_link_directories(SPPVideo 
	PRIVATE		
		"${THIRD_PARTY}/libav/lib" )		
			
target_link_libraries( SPPVideo 
	PUBLIC
		SPPCore )		

target_link_libraries( SPPVideo 
	PRIVATE
		avcodec
		avformat
		avresample
		avutil
		swscale)

install(TARGETS SPPVideo DESTINATION "${SPP_ROOT}/INSTALL")

if(USE_CUDA)

############################################################
#CUDA
############################################################
add_library(SPPVideo_CUDA SHARED "")
set_target_properties(SPPVideo_CUDA PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
add_dependencies( SPPVideo_CUDA SPPCore )

# will need define to not create empty dll export
target_compile_definitions(SPPVideo_CUDA
	PRIVATE
		SPP_VIDEO_EXPORT=1
		WITH_CUDA=1)
				
target_sources(SPPVideo_CUDA
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPVideo.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPVideo.h"				
		)

target_include_directories(SPPVideo_CUDA 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )

target_include_directories(SPPVideo_CUDA 
	PRIVATE  	
		"${THIRD_PARTY}/libav_CUDA/include" )
		
target_link_directories(SPPVideo_CUDA 
	PRIVATE		
		"${THIRD_PARTY}/libav_CUDA/lib" )		
			
target_link_libraries( SPPVideo_CUDA 
	PUBLIC
		SPPCore )		

target_link_libraries( SPPVideo_CUDA 
	PRIVATE
		avcodec
		avformat
		avresample
		avutil
		swscale)

install(TARGETS SPPVideo_CUDA DESTINATION "${SPP_ROOT}/INSTALL")

endif()



