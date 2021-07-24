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
		"${CMAKE_CURRENT_LIST_DIR}/SPPObject.h" )

target_include_directories(SPPObject 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )

target_link_libraries( SPPObject 
	PUBLIC
		SPPCore )

install(TARGETS SPPObject DESTINATION "${SPP_ROOT}/INSTALL")



