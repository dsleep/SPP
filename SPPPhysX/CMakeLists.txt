cmake_minimum_required(VERSION 3.5)
project(SPPPhysx_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPPhysx SHARED "")

set_target_properties(SPPPhysx PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies(SPPPhysx SPPCore )

# will need define to not create empty dll export
target_compile_definitions(SPPPhysx
	PRIVATE
		SPP_PHYSX_EXPORT=1)
				
target_sources(SPPPhysx
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPPhysX.h"
		"${CMAKE_CURRENT_LIST_DIR}/SPPPhysX.cpp" 
		)

target_include_directories(SPPPhysx 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )
			
target_link_libraries(SPPPhysx 
	PUBLIC
		SPPCore 
		)
		
target_include_directories(SPPPhysx 
	PRIVATE  			
		"${THIRD_PARTY}/PhysX1.4/PhysX/include"
		"${THIRD_PARTY}/PhysX1.4/PxShared/include" )
		
target_link_directories(SPPPhysx 
	PRIVATE
		debug "${THIRD_PARTY}/PhysX1.4/PhysX/bin/win.x86_64.vc143.md/debug" optimized "${THIRD_PARTY}/PhysX1.4/PhysX/bin/win.x86_64.vc143.md/release" )
		
target_link_libraries(SPPPhysx 
	PRIVATE
		PhysX_64 
		PhysXCommon_64
		PhysXCooking_64
		PhysXFoundation_64
		PhysXCharacterKinematic_static_64
		PhysXTask_static_64 
		PhysXVehicle_static_64 
		PhysXExtensions_static_64 
		PhysXPvdSDK_static_64 )

install(TARGETS SPPPhysx DESTINATION "${SPP_ROOT}/INSTALL")



