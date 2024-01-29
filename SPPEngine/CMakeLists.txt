cmake_minimum_required(VERSION 3.5)
project(SPPEngine_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPEngine SHARED "")

set_target_properties(SPPEngine PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPEngine SPPCore SPPMath SPPReflection )


# will need define to not create empty dll export
target_compile_definitions(SPPEngine
	PRIVATE
		SPP_ENGINE_EXPORT=1)
				
target_sources(SPPEngine
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPApplication.h"
		"${CMAKE_CURRENT_LIST_DIR}/SPPEngine.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPEngine.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPAssetCache.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPAssetCache.h"

		"${CMAKE_CURRENT_LIST_DIR}/SPPPythonInterface.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPPythonInterface.h"
		)
        
if(OS_WINDOWS)

target_sources(SPPEngine
    PRIVATE
		
        "${CMAKE_CURRENT_LIST_DIR}/Win32/Win32Application.cpp"
		)

endif()

target_include_directories(SPPEngine 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}"
		"${THIRD_PARTY}/eigen/include/eigen3" )
		
target_include_directories(SPPEngine 
	PRIVATE  	
		"$ENV{PYTHON}/include" )
		
		
target_link_directories(SPPEngine 
	PRIVATE
		"$ENV{PYTHON}/libs" )		
	
target_link_libraries( SPPEngine 
	PUBLIC
		SPPCore 
		SPPMath
		SPPReflection
		)

install(TARGETS SPPEngine DESTINATION "${SPP_ROOT}/INSTALL")



