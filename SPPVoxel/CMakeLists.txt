cmake_minimum_required(VERSION 3.5)
project(SPPVOXEL_API VERSION 1.0.0 LANGUAGES CXX)


add_library(SPPVoxel SHARED "")
set_target_properties(SPPVoxel PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
add_dependencies( SPPVoxel SPPEngine )

# will need define to not create empty dll export
target_compile_definitions(SPPVoxel
	PRIVATE
		SPP_VOXEL_EXPORT=1)
		
target_sources(SPPVoxel
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPVoxel.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPVoxel.h"
		 )
		
target_link_libraries( SPPVoxel 
	PUBLIC		
		SPPEngine)
	
target_include_directories(SPPVoxel 
	PRIVATE  	
		"${THIRD_PARTY}/PolyVox" )

install(TARGETS SPPVoxel DESTINATION "${SPP_ROOT}/INSTALL")
