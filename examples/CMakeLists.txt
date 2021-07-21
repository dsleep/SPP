cmake_minimum_required(VERSION 3.5)
project(SPPTestApp_API VERSION 1.0.0 LANGUAGES CXX)

#testbasicapp
add_executable( testbasicapp WIN32 "")
add_dependencies( testbasicapp SPPGraphics )			
target_sources( testbasicapp 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/testbasicapp.cpp" )		
target_link_libraries( testbasicapp SPPGraphics )
set_target_properties(testbasicapp PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET testbasicapp PROPERTY FOLDER "TEST")

#testeditor
add_executable( testeditor WIN32 "")
add_dependencies( testeditor SPPGraphics SPPCEFUserInterface CEFProcess )				
target_sources( testeditor 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/testeditor.cpp"		
		"${SPP_ROOT}/SPPCEFUI/cefclient/resources/win/cefclient.rc"  )		
target_link_libraries( testeditor SPPGraphics SPPCEFUserInterface )		
set_target_properties(testeditor PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET testeditor PROPERTY FOLDER "TEST")

#testrasterizers
add_executable( testrasterizers WIN32 "")
add_dependencies( testrasterizers SPPGraphics )			
target_sources( testrasterizers 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/testrasterizers.cpp" )		
target_link_libraries( testrasterizers SPPGraphics )
set_target_properties( testrasterizers PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET testrasterizers PROPERTY FOLDER "TEST")

# #testterrain
# add_executable( testterrain WIN32 "")
# add_dependencies( testterrain SPPGraphics )				
# target_sources( testterrain PRIVATE "${CMAKE_CURRENT_LIST_DIR}/testterrain.cpp" )		
# target_link_libraries( testterrain SPPGraphics )


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