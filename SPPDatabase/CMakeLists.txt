cmake_minimum_required(VERSION 3.5)
project(SPPDatabase_API VERSION 1.0.0 LANGUAGES CXX C)

add_library(SPPDatabase SHARED "")

set_target_properties(SPPDatabase PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPDatabase SPPCore )

# will need define to not create empty dll export
target_compile_definitions(SPPDatabase
	PRIVATE
		SPP_DATABASE_EXPORT=1)
				
target_sources(SPPDatabase
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPDatabase.h"			
		"${CMAKE_CURRENT_LIST_DIR}/SPPDatabase.cpp"		
		"${CMAKE_CURRENT_LIST_DIR}/sqllite/sqlite3.h"
		"${CMAKE_CURRENT_LIST_DIR}/sqllite/sqlite3.c"		
		)

target_include_directories(SPPDatabase 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )
					
target_link_libraries(SPPDatabase 
	PUBLIC
		SPPCore )

install(TARGETS SPPDatabase DESTINATION "${SPP_ROOT}/INSTALL")



