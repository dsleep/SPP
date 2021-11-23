cmake_minimum_required(VERSION 3.5)
project(SPPOpenGL_API VERSION 1.0.0 LANGUAGES CXX)

add_definitions(-DUSE_OPENGL=1)

add_library(SPPOpenGL SHARED "")

set_target_properties(SPPOpenGL PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPOpenGL SPPEngine )

# will need define to not create empty dll export
target_compile_definitions( SPPOpenGL
	PRIVATE
		SPP_OPENGL_EXPORT=1
		GLEW_STATIC=1)
				
target_sources( SPPOpenGL
	PRIVATE			
		"${CMAKE_CURRENT_LIST_DIR}/OpenGLBuffers.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/OpenGLBuffers.h"
		"${CMAKE_CURRENT_LIST_DIR}/OpenGLDevice.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/OpenGLDevice.h"
		"${CMAKE_CURRENT_LIST_DIR}/OpenGLShaders.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/OpenGLShaders.h"
		"${CMAKE_CURRENT_LIST_DIR}/OpenGLTextures.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/OpenGLTextures.h"
		"${CMAKE_CURRENT_LIST_DIR}/OpenGLUtils.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/OpenGLUtils.h"
		)

target_include_directories(SPPOpenGL 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )
		
target_include_directories(SPPOpenGL 
	PRIVATE  			
		"${THIRD_PARTY}/glew/include" )
		
target_link_directories(SPPOpenGL 
	PRIVATE
		"${THIRD_PARTY}/glew/lib")		
		
		
target_link_libraries( SPPOpenGL 
	PUBLIC
		SPPEngine
		SPPGraphics	)
		
target_link_libraries( SPPOpenGL 
	PRIVATE
		debug "Debug/x64/glew32sd" optimized "Release/x64/glew32s"
		opengl32 )

install(TARGETS SPPOpenGL DESTINATION "${SPP_ROOT}/INSTALL")



