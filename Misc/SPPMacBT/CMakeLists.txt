cmake_minimum_required(VERSION 3.5)
project(SPPMacBT_API VERSION 1.0.0 LANGUAGES CXX OBJCXX)

enable_language(OBJCXX)

add_library(SPPMacBT SHARED "")

set_target_properties(SPPMacBT PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPMacBT SPPCore )

# will need define to not create empty dll export
target_compile_definitions(SPPMacBT
	PRIVATE
		SPP_WINRTBTW_EXPORT=1)
		
# target_compile_definitions(SPPMacBT
	# PUBLIC
		# UNICODE 
		# _UNICODE
		# NOMINMAX)		

target_sources(SPPMacBT
	PRIVATE					
		"${CMAKE_CURRENT_LIST_DIR}/SPPMacBT.mm"
		"${CMAKE_CURRENT_LIST_DIR}/SPPMacBT.h"
        "${CMAKE_CURRENT_LIST_DIR}/SPPMacBTDelegate.h" 
		)		

target_include_directories(SPPMacBT 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )

target_link_libraries( SPPMacBT 
	PUBLIC
		SPPCore
        "-framework CoreBluetooth" )
		
# target_link_libraries(SPPMacBT 
	# PUBLIC
		# debug rttr_core_d optimized rttr_core )		

install(TARGETS SPPMacBT DESTINATION "${SPP_ROOT}/INSTALL")

