cmake_minimum_required(VERSION 3.5)
project(SPPTestApp_API VERSION 1.0.0 LANGUAGES CXX)

add_executable( testnat "")
add_dependencies( testnat SPPNetworking )			
target_sources( testnat 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/testnat.cpp" )		
target_link_libraries( testnat SPPNetworking )
set_target_properties( testnat PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET testnat PROPERTY FOLDER "TEST")

add_executable( testvideo "")
add_dependencies( testvideo SPPVideo_CUDA SPPCapture )			
target_sources( testvideo 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/testvideo.cpp" )		
target_link_libraries( testvideo SPPVideo_CUDA SPPCapture )
set_target_properties( testvideo PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET testvideo PROPERTY VS_DPI_AWARE "PerMonitor")
set_property(TARGET testvideo PROPERTY FOLDER "TEST")

set_target_properties(testvideo PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/DELAYLOAD:SPPVideo_CUDA.dll" )
set_target_properties(testvideo PROPERTIES LINK_FLAGS_RELEASE "/DELAYLOAD:SPPVideo_CUDA.dll" )
set_target_properties(testvideo PROPERTIES LINK_FLAGS_DEBUG "/DELAYLOAD:SPPVideo_CUDAd.dll" )