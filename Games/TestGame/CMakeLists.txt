cmake_minimum_required(VERSION 3.5)
project(SPPTestGame_API VERSION 1.0.0 LANGUAGES CXX)

add_executable( testGame WIN32 "")
add_dependencies( testGame SPPGameEngine SPPSDFO SPPAnimation SPPXAudio2 ${SPPGDI} )			
target_sources( testGame 
	PRIVATE 
		"${CMAKE_CURRENT_LIST_DIR}/testGame.cpp" )		
target_link_libraries( testGame 
	PRIVATE
		SPPGameEngine SPPSDFO SPPAnimation SPPXAudio2 )
set_target_properties( testGame PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
set_property(TARGET testGame PROPERTY FOLDER "GAMES")
set_property(TARGET testGame PROPERTY VS_DPI_AWARE "PerMonitor")
