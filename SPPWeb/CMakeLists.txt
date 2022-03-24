cmake_minimum_required(VERSION 3.5)
project(SPPWEB_API VERSION 1.0.0 LANGUAGES CXX)


add_executable(SPPWeb "")
add_dependencies( SPPWeb SPPEngine )

set_target_properties( SPPWeb PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET SPPWeb PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET SPPWeb PROPERTY VS_DPI_AWARE "PerMonitor")

target_sources(SPPWeb
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPWeb.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPWeb.h"
		
		"${THIRD_PARTY}/civetweb/include/CivetServer.cpp" )
		
target_link_libraries( SPPWeb 
	PUBLIC		
		SPPEngine)
		
target_link_directories(SPPWeb 
	PRIVATE		
		"${THIRD_PARTY}/civetweb/lib"
		)

target_link_libraries(SPPWeb 
	PRIVATE
        debug civetwebd optimized civetweb
		)
		
target_include_directories(SPPWeb 
	PRIVATE  	
		"${THIRD_PARTY}/civetweb/include" )

install(TARGETS SPPWeb DESTINATION "${SPP_ROOT}/INSTALL")
