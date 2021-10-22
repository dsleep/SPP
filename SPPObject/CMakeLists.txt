cmake_minimum_required(VERSION 3.5)
project(SPPObject_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPObject SHARED "")

set_target_properties(SPPObject PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPObject SPPCore SPPReflection )

# will need define to not create empty dll export
target_compile_definitions(SPPObject
	PRIVATE
		SPP_OBJECT_EXPORT=1)

target_sources(SPPObject
	PRIVATE					
		"${CMAKE_CURRENT_LIST_DIR}/SPPObject.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPObject.h" 
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPObjectJSON.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPObjectJSON.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPGarbageCollection.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPGarbageCollection.h"
		)

target_include_directories(SPPObject 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )

target_link_libraries( SPPObject 
	PUBLIC
		SPPCore 
		SPPReflection )
		
install(TARGETS SPPObject DESTINATION "${SPP_ROOT}/INSTALL")

#####

#add_executable( ObjectGenerator "")
#add_dependencies( ObjectGenerator SPPCore SPPReflection SPPObject)			
#target_sources( ObjectGenerator 
	#PRIVATE 
		#"${CMAKE_CURRENT_LIST_DIR}/ObjectGenerator.cpp" )		
#target_link_libraries( ObjectGenerator SPPCore SPPReflection SPPObject )
#set_target_properties( ObjectGenerator PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
#set_property(TARGET ObjectGenerator PROPERTY FOLDER "APPLICATIONS")
#set_property(TARGET ObjectGenerator PROPERTY VS_DPI_AWARE "PerMonitor")
#
