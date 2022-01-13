cmake_minimum_required(VERSION 3.5)
project(SPPReflection_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPReflection SHARED "")

set_target_properties(SPPReflection PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPReflection SPPCore )

# will need define to not create empty dll export
target_compile_definitions(SPPReflection
	PRIVATE
		SPP_REFLECTION_EXPORT=1)
		
target_compile_definitions(SPPReflection
	PUBLIC
		RTTR_DLL
		UNICODE 
		_UNICODE
		NOMINMAX)		

target_sources(SPPReflection
	PRIVATE					
		"${CMAKE_CURRENT_LIST_DIR}/SPPReflection.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPReflection.h" 
		)

target_include_directories(SPPReflection 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}"
		"${THIRD_PARTY}/rttr/include" )

target_link_directories(SPPReflection 
	PUBLIC
		"${THIRD_PARTY}/rttr/lib" )
		
target_link_libraries( SPPReflection 
	PUBLIC
		SPPCore )
		
target_link_libraries(SPPReflection 
	PUBLIC
		debug rttr_core_d optimized rttr_core )		

install(TARGETS SPPReflection DESTINATION "${SPP_ROOT}/INSTALL")

if(OS_WINDOWS)
	install(FILES "${THIRD_PARTY}/rttr/bin/rttr_core.dll" DESTINATION "${SPP_ROOT}/INSTALL")
endif()

if(OS_LINUX)

install(DIRECTORY "${THIRD_PARTY}/rttr/lib/" #trailing / matters to flatten it
	DESTINATION "${SPP_ROOT}/INSTALL"
	FILES_MATCHING 
		PATTERN "*.so.*"
		PATTERN mswu EXCLUDE 
		PATTERN mswud EXCLUDE)

endif()
