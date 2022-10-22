cmake_minimum_required(VERSION 3.5)
project(SPPPhysX_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPPhysX SHARED "")

set_target_properties(SPPPhysX PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies(SPPPhysX SPPCore SPPMath)

# will need define to not create empty dll export
target_compile_definitions(SPPPhysX
	PRIVATE
		SPP_PHYSX_EXPORT=1)
				
target_sources(SPPPhysX
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPPhysX.h"
		"${CMAKE_CURRENT_LIST_DIR}/SPPPhysX.cpp" 
		)

target_include_directories(SPPPhysX 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )
			
target_link_libraries(SPPPhysX 
	PUBLIC
		SPPCore 
		SPPMath
		
		)
		
target_include_directories(SPPPhysX 
	PRIVATE  			
		"${THIRD_PARTY}/PhysX1.4/PhysX/include"
		"${THIRD_PARTY}/PhysX1.4/PxShared/include" )
		
target_link_directories(SPPPhysX 
	PRIVATE
		"${THIRD_PARTY}/PhysX1.4/PhysX/bin/win.x86_64.vc143.md" )
		
list(APPEND 
		PhysxDependencies 
			"PhysX_64"
			"PhysXCommon_64"
			"PhysXCooking_64"
			"PhysXFoundation_64"
			"PhysXCharacterKinematic_static_64"
			"PhysXTask_static_64"
			"PhysXVehicle_static_64"
			"PhysXExtensions_static_64"
			"PhysXPvdSDK_static_64" )

foreach(curDepend ${PhysxDependencies})
	target_link_libraries(SPPPhysX 
		PRIVATE
			debug "debug/${curDepend}" optimized  "release/${curDepend}" )
endforeach()

install(TARGETS SPPPhysX DESTINATION "${SPP_ROOT}/INSTALL")

if(OS_WINDOWS)

	set(delayLoads "")

	foreach(curDepend ${PhysxDependencies})
		set(delayLoads "${delayLoads} /DELAYLOAD:${curDepend}.dll")
	endforeach()

	set_target_properties(SPPPhysX PROPERTIES LINK_FLAGS_RELWITHDEBINFO "${delayLoads}" )
	set_target_properties(SPPPhysX PROPERTIES LINK_FLAGS_RELEASE "${delayLoads}" )
	set_target_properties(SPPPhysX PROPERTIES LINK_FLAGS_DEBUG "${delayLoads}" )

endif() 



