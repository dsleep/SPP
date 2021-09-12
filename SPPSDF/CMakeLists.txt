cmake_minimum_required(VERSION 3.5)
project(SPPSDF_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPSDF SHARED "")

set_target_properties(SPPSDF PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPSDF SPPMath SPPObject SPPScene )

# will need define to not create empty dll export
target_compile_definitions(SPPSDF
	PRIVATE
		SPP_SDF_EXPORT=1)
		
target_include_directories(SPPSDF 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )

target_sources(SPPSDF
	PRIVATE					
		"${CMAKE_CURRENT_LIST_DIR}/SPPSDF.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPSDF.h" 		
		)
				
target_link_libraries( SPPSDF 
	PUBLIC
		SPPObject
		SPPMath
		SPPScene )
		
install(TARGETS SPPSDF DESTINATION "${SPP_ROOT}/INSTALL")

