# Copyright (c) David Sleeper (Sleeping Robot LLC)

# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
# to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
# and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
# DEALINGS IN THE SOFTWARE.

cmake_minimum_required(VERSION 3.5)
project(SPPGLOBAL VERSION 1.0.0 LANGUAGES CXX C)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY OS_FOLDERS ON)

LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

message( STATUS "BUILD SYSTEM: ${CMAKE_SYSTEM}" )
message( STATUS "BUILD SYSTEM NAME: ${CMAKE_SYSTEM_NAME}" ) 
message( STATUS "BUILD SYSTEM PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}" )

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(OS_LINUX ON)
	message(STATUS "LINUX FOUND")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(OS_WINDOWS ON)
	message(STATUS "WINDOWS FOUND")
else()
	message(FATAL_ERROR "We do not know OS!!!")
endif()
	
set(SPP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" )
set(THIRD_PARTY "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty" )	
set(LIB_PATH_POSTFIX "win64/vc16" )	
set(CMAKE_DEBUG_POSTFIX "d" )	
	
set(USE_VS_MULTIPROCESS ON CACHE BOOL "Use MultiProcessor compiling on VS")

#General C++ Compiling
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
	
	if(USE_VS_MULTIPROCESS)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
	endif()
	
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_SOURCE_DIR}/Binaries/ReleaseWithDebug/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_SOURCE_DIR}/Binaries)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_SOURCE_DIR}/Binaries)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Release/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/Binaries)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/Binaries)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Debug/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/Binaries)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/Binaries)

set(SPP_TARGET_OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Binaries)

include_directories( "${THIRD_PARTY}" )

list(APPEND SPPProjects 
	"SPPCore" "SPPDatabase" "SPPNetworking" "SPPCapture" 
	"SPPVideo" "SPPObject" "SPPEngine" "SPPGraphics" "SPPCEF"
	"SPPDX12" 
	"applications" 
	"RemoteControllerWPF" "RemoteApplicationWPF" "ApplicationThroughputWPF")

# set working directory
function(add_executable target_name)
	# Call the original function
	_add_executable(${target_name} ${ARGN})
	message( STATUS "add_executable ${target_name}" )
	if (WIN32)
		set_property(TARGET ${target_name} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${SPP_ROOT}/Binaries")
	endif()
endfunction(add_executable target_name)


foreach(subdir ${SPPProjects})	
	message( STATUS "Adding Project: ${subdir}" )
    add_subdirectory(${subdir})
endforeach()

