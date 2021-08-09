cmake_minimum_required(VERSION 3.5)
project(SPPCEFUI_API VERSION 1.0.0 LANGUAGES CXX)


include(cef_macros)

add_library(SPPCEFUserInterface SHARED "")
add_dependencies( SPPCEFUserInterface SPPEngine )

set_target_properties( SPPCEFUserInterface PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

# will need define to not create empty dll export
target_compile_definitions(SPPCEFUserInterface
	PRIVATE
		SPP_CEFUI_EXPORT=1
		WINVER=0x0601
		_WIN32_WINNT=0x601
		NOMINMAX
		WIN32_LEAN_AND_MEAN
		NOMINMAX
		__PRETTY_FUNCTION__=__FUNCTION__
		_WINSOCK_DEPRECATED_NO_WARNINGS
		HAVE_JPEG
		_SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING)

target_sources(SPPCEFUserInterface
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPCEFUI.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPCEFUI.h"	)

target_include_directories(SPPCEFUserInterface 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}"		
		"${THIRD_PARTY}/jsoncpp/include"
		"${THIRD_PARTY}/cef" )
		
target_link_directories(SPPCEFUserInterface 
	PRIVATE
		"${THIRD_PARTY}/cef"
		"${THIRD_PARTY}/jsoncpp/lib/${LIB_PATH_POSTFIX}")		
		
target_link_libraries( SPPCEFUserInterface 
	PUBLIC		
		SPPEngine)

target_link_libraries( SPPCEFUserInterface 
	PRIVATE				
		jsoncppd
		# CEF FILES
		debug "Debug/libcef" optimized "Release/libcef"
		debug "Debug/libcef_dll_wrapper" optimized "Release/libcef_dll_wrapper"
		comctl32.lib
		rpcrt4.lib
		shlwapi.lib
		ws2_32.lib
		opengl32.lib
		d3d11.lib 
		glu32.lib 
		imm32.lib
		d3d12
		dxgi
		d3dcompiler
		dxguid )

install(TARGETS SPPCEFUserInterface DESTINATION "${SPP_ROOT}/INSTALL")

# add CEF client, at this time was 90.5.3
message( STATUS "Adding CEF Client" )
add_subdirectory(cefclient)

foreach(CURELEMENT IN LISTS CEFCLIENT_SRCS)
	message( STATUS "Adding CEF ${CURELEMENT}" )
	target_sources(SPPCEFUserInterface
		PRIVATE
			"${CMAKE_CURRENT_LIST_DIR}/cefclient/${CURELEMENT}" )
endforeach()

# CEFProcess
add_executable( CEFProcess 
	WIN32 
		"${CMAKE_CURRENT_LIST_DIR}/SPPCEF_win.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/cefclient/resources/win/cefclient.rc" 
		)
add_dependencies( CEFProcess SPPCEFUserInterface )
target_link_libraries( CEFProcess SPPCEFUserInterface )

ADD_WINDOWS_MANIFEST("${CMAKE_CURRENT_SOURCE_DIR}/cefclient/resources/win" "CEFProcess" "exe")