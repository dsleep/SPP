cmake_minimum_required(VERSION 3.5)
project(SPPObject_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPObject SHARED "")

set_target_properties(SPPObject PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPObject SPPCore )

# will need define to not create empty dll export
target_compile_definitions(SPPObject
	PRIVATE
		SPP_OBJECT_EXPORT=1)
		
target_compile_definitions(SPPObject
	PUBLIC
		UNICODE 
		_UNICODE
		NOMINMAX)		

target_sources(SPPObject
	PRIVATE					
		"${CMAKE_CURRENT_LIST_DIR}/SPPObject.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPObject.h" 
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPField.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPField.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPFunction.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPFunction.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPStruct.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPStruct.h"
		)

target_include_directories(SPPObject 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}"
		"${THIRD_PARTY}/rttr/include" )

target_link_directories(SPPObject 
	PRIVATE
		"${THIRD_PARTY}/rttr" )
		
target_link_libraries( SPPObject 
	PUBLIC
		SPPCore )
		
target_link_libraries(SPPObject 
	PRIVATE
		debug rttr_core_d optimized rttr_core )		

install(TARGETS SPPObject DESTINATION "${SPP_ROOT}/INSTALL")

#####

add_executable( ObjectGenerator "")
add_dependencies( ObjectGenerator SPPCore SPPObject)			
target_sources( ObjectGenerator 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/ObjectGenerator.cpp" )		
target_link_libraries( ObjectGenerator SPPCore SPPObject )
set_target_properties( ObjectGenerator PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET ObjectGenerator PROPERTY FOLDER "APPLICATIONS")
set_property(TARGET ObjectGenerator PROPERTY VS_DPI_AWARE "PerMonitor")

