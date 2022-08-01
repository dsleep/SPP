cmake_minimum_required(VERSION 3.5)
project(SPPXAudio2_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPXAudio2 SHARED "")

set_target_properties(SPPXAudio2 PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies(SPPXAudio2 SPPCore SPPMath SPPSceneO)

# will need define to not create empty dll export
target_compile_definitions(SPPXAudio2
	PRIVATE
		SPP_XAUDIO2_EXPORT=1)
				
target_sources(SPPXAudio2
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPXAudio2.h"
		"${CMAKE_CURRENT_LIST_DIR}/SPPXAudio2.cpp" 
		
		"${CMAKE_CURRENT_LIST_DIR}/WAVFileReader.cpp" 
		"${CMAKE_CURRENT_LIST_DIR}/WAVFileReader.h" 				
		)

target_include_directories(SPPXAudio2 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )
			
target_link_libraries(SPPXAudio2 
	PUBLIC
		SPPCore 
		SPPMath
		SPPSceneO
		)
		
# target_include_directories(SPPXAudio2 
	# PRIVATE  			
		# "${THIRD_PARTY}/PhysX1.4/PhysX/include"
		# "${THIRD_PARTY}/PhysX1.4/PxShared/include" )
		
# target_link_directories(SPPXAudio2 
	# PRIVATE
		# debug "${THIRD_PARTY}/PhysX1.4/PhysX/bin/win.x86_64.vc143.md/debug" optimized "${THIRD_PARTY}/PhysX1.4/PhysX/bin/win.x86_64.vc143.md/release" )
		
# list(APPEND 
		# PhysxDependencies 
			# "PhysX_64"
			# "PhysXCommon_64"
			# "PhysXCooking_64"
			# "PhysXFoundation_64"
			# "PhysXCharacterKinematic_static_64"
			# "PhysXTask_static_64"
			# "PhysXVehicle_static_64"
			# "PhysXExtensions_static_64"
			# "PhysXPvdSDK_static_64" )

# target_link_libraries(SPPPhysX 
	# PRIVATE
		# ${PhysxDependencies} )

# install(TARGETS SPPPhysX DESTINATION "${SPP_ROOT}/INSTALL")

# if(OS_WINDOWS)

	# set(delayLoads "")

	# foreach(curDepend ${PhysxDependencies})
		# set(delayLoads "${delayLoads} /DELAYLOAD:${curDepend}.dll")
	# endforeach()

	# set_target_properties(SPPPhysX PROPERTIES LINK_FLAGS_RELWITHDEBINFO "${delayLoads}" )
	# set_target_properties(SPPPhysX PROPERTIES LINK_FLAGS_RELEASE "${delayLoads}" )
	# set_target_properties(SPPPhysX PROPERTIES LINK_FLAGS_DEBUG "${delayLoads}" )

# endif() 



