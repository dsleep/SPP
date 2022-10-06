cmake_minimum_required(VERSION 3.5)
project(SPPPython_API VERSION 1.0.0 LANGUAGES CXX)

#ITS ugly but it is what it is CUDA copy below
add_library(SPPPython SHARED "")
set_target_properties(SPPPython PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
add_dependencies( SPPPython SPPCore )

# will need define to not create empty dll export
target_compile_definitions(SPPPython
	PRIVATE
		SPP_PYTHON_EXPORT=1)
				
target_sources(SPPPython
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPPython.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPPython.h"				
		)

target_include_directories(SPPPython 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )

target_include_directories(SPPPython 
	PRIVATE  	
		"$ENV{PYTHON}/include" )
		
target_link_directories(SPPPython 
	PRIVATE		
		"$ENV{PYTHON}/libs"
		"$ENV{PYTHON}" )		
			
target_link_libraries( SPPPython 
	PUBLIC
		SPPCore )		

target_link_libraries( SPPPython 
	PRIVATE
		debug python39_d optimized python39 )

install(TARGETS SPPPython DESTINATION "${SPP_ROOT}/INSTALL")

#if(OS_WINDOWS)
#	set_target_properties(SPPPython PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/DELAYLOAD:python39.dll" )
#	set_target_properties(SPPPython PROPERTIES LINK_FLAGS_RELEASE "/DELAYLOAD:python39.dll" )
#	set_target_properties(SPPPython PROPERTIES LINK_FLAGS_DEBUG "/DELAYLOAD:python39_d.dll" )
#endif() 
