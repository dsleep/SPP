cmake_minimum_required(VERSION 3.5)
project(SPPMath_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPMath SHARED "")

set_target_properties(SPPMath PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPMath SPPCore )


# will need define to not create empty dll export
target_compile_definitions(SPPMath
	PRIVATE
		SPP_MATH_EXPORT=1)
				
target_sources(SPPMath
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPMath.h"
		"${CMAKE_CURRENT_LIST_DIR}/SPPMath.cpp" 
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPPrimitiveShapes.h"
		"${CMAKE_CURRENT_LIST_DIR}/SPPPrimitiveShapes.cpp"		
		)

target_include_directories(SPPMath 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}"
		"${THIRD_PARTY}/eigen/include/eigen3" )
			
target_link_libraries( SPPMath 
	PUBLIC
		SPPCore 
		)

install(TARGETS SPPMath DESTINATION "${SPP_ROOT}/INSTALL")



