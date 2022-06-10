cmake_minimum_required(VERSION 3.5)
project(SPPWEBSERVER_API VERSION 1.0.0 LANGUAGES CXX)


add_executable(SPPWebServer "")
add_dependencies( SPPWebServer SPPEngine )

set_target_properties( SPPWebServer PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET SPPWebServer PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET SPPWebServer PROPERTY VS_DPI_AWARE "PerMonitor")

target_sources(SPPWebServer
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPWeb.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPWeb.h"
		
		"${THIRD_PARTY}/civetweb/include/CivetServer.cpp" )
		
target_link_libraries( SPPWebServer 
	PUBLIC		
		SPPEngine)
		
target_link_directories(SPPWebServer 
	PRIVATE		
		"${THIRD_PARTY}/civetweb/lib"
		)

target_link_libraries(SPPWebServer 
	PRIVATE
        debug civetwebd optimized civetweb
		)
		
target_include_directories(SPPWebServer 
	PRIVATE  	
		"${THIRD_PARTY}/civetweb/include" )

install(TARGETS SPPWebServer DESTINATION "${SPP_ROOT}/INSTALL")
