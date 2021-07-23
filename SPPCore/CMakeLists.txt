cmake_minimum_required(VERSION 3.5)
project(SPPCore_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPCore SHARED "")

set_target_properties(SPPCore PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

# will need define to not create empty dll export
target_compile_definitions(SPPCore
	PRIVATE
		SPP_CORE_EXPORT=1)
		
target_compile_definitions(SPPCore
	PUBLIC
		UNICODE 
		_UNICODE
		NOMINMAX
		WIN32_LEAN_AND_MEAN)		

target_sources(SPPCore
	PRIVATE	
		"${CMAKE_CURRENT_LIST_DIR}/SPPCore.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPCore.h"

		"${CMAKE_CURRENT_LIST_DIR}/SPPSTLUtils.h"

		"${CMAKE_CURRENT_LIST_DIR}/SPPLogging.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPLogging.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPSerialization.cpp"	
		"${CMAKE_CURRENT_LIST_DIR}/SPPSerialization.h"		
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPFileSystem.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPFileSystem.h"
				
		"${CMAKE_CURRENT_LIST_DIR}/SPPWin32Core.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPWin32Core.h"
				
		"${CMAKE_CURRENT_LIST_DIR}/SPPSpan.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPJsonUtils.h"
		"${CMAKE_CURRENT_LIST_DIR}/SPPJsonUtils.cpp"
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPString.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPString.h"

		"${CMAKE_CURRENT_LIST_DIR}/SPPMemory.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPMemory.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPNumberedString.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPNumberedString.h"

		"${CMAKE_CURRENT_LIST_DIR}/ThreadPool.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/ThreadPool.h"
		"${CMAKE_CURRENT_LIST_DIR}/SPPTiming.h"
		"${CMAKE_CURRENT_LIST_DIR}/SPPArrayResource.h" )

target_include_directories(SPPCore 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}"		
		"${THIRD_PARTY}/jsoncpp/include" )

target_link_directories(SPPCore 
	PUBLIC		
		"${THIRD_PARTY}/jsoncpp/lib/${LIB_PATH_POSTFIX}"
		)

target_link_libraries(SPPCore 
	PUBLIC
		debug jsoncppd optimized jsoncpp 
		)

install(TARGETS SPPCore DESTINATION "${SPP_ROOT}/INSTALL")



