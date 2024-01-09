cmake_minimum_required(VERSION 3.5)
project(SPPWinRTBTE_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPWinRTBTE SHARED "")

set_target_properties(SPPWinRTBTE PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPWinRTBTE SPPCore )

# will need define to not create empty dll export
target_compile_definitions(SPPWinRTBTE
	PRIVATE
		SPP_WINRTBTW_EXPORT=1)
		
# target_compile_definitions(SPPWinRTBTE
	# PUBLIC
		# UNICODE 
		# _UNICODE
		# NOMINMAX)		

target_sources(SPPWinRTBTE
	PRIVATE					
		"${CMAKE_CURRENT_LIST_DIR}/SPPWinRTBTE.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPWinRTBTE.h" 
		)
		
target_compile_options( SPPWinRTBTE 
	PRIVATE 
		"/await"
		"/ZW"
		"/AI\"C:/Program Files (x86)/Windows Kits/10/UnionMetadata/10.0.22621.0\""
		"/AI\"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.37.32822/lib/x86/store/references\""
	)



# set_source_files_properties(SPPWinRTBTE.cpp PROPERTIES COMPILE_FLAGS "/await" )
# set_source_files_properties(SPPWinRTBTE.cpp PROPERTIES COMPILE_FLAGS "/ZW" )
# set_source_files_properties(SPPWinRTBTE.cpp PROPERTIES COMPILE_FLAGS "/AI\"C:/Program Files (x86)/Windows Kits/10/UnionMetadata/10.0.19041.0\"" )
# set_source_files_properties(SPPWinRTBTE.cpp PROPERTIES COMPILE_FLAGS "/AI\"C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC/14.29.30133/lib/x86/store/references\"" )

target_include_directories(SPPWinRTBTE 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )

# target_link_directories(SPPWinRTBTE 
	# PUBLIC
		# "${THIRD_PARTY}/rttr/lib" )
		
target_link_libraries( SPPWinRTBTE 
	PUBLIC
		SPPCore )
		
# target_link_libraries(SPPWinRTBTE 
	# PUBLIC
		# debug rttr_core_d optimized rttr_core )		

install(TARGETS SPPWinRTBTE DESTINATION "${SPP_ROOT}/INSTALL")

