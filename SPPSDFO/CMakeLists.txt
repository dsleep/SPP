cmake_minimum_required(VERSION 3.5)
project(SPPSDFO_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPSDFO SHARED "")

set_target_properties(SPPSDFO PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPSDFO SPPCore SPPObject SPPMath SPPSceneO SPPGraphics SPPGraphicsO )

# will need define to not create empty dll export
target_compile_definitions(SPPSDFO
	PRIVATE
		SPP_SDFO_EXPORT=1)
		
target_include_directories(SPPSDFO 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )

target_sources(SPPSDFO
	PRIVATE					
		"${CMAKE_CURRENT_LIST_DIR}/SPPSDFO.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPSDFO.h" 				
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPLoadMagicaCSG.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPLoadMagicaCSG.h"
		)
				
target_link_libraries( SPPSDFO 
	PUBLIC
		SPPCore 
		SPPObject 
		SPPMath 
		SPPSceneO 
		SPPGraphics 
		SPPGraphicsO )
		
install(TARGETS SPPSDFO DESTINATION "${SPP_ROOT}/INSTALL")

