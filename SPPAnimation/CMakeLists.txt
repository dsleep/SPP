cmake_minimum_required(VERSION 3.5)
project(SPPAnimation_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPAnimation SHARED "")

set_target_properties(SPPAnimation PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies(SPPAnimation SPPCore SPPMath SPPSceneO)

# will need define to not create empty dll export
target_compile_definitions(SPPAnimation
	PRIVATE
		SPP_ANIMATION_EXPORT=1)
				
target_sources(SPPAnimation
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPAnimation.h"
		"${CMAKE_CURRENT_LIST_DIR}/SPPAnimation.cpp" 				
		)

target_include_directories(SPPAnimation 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )
			
target_link_libraries(SPPAnimation 
	PUBLIC
		SPPCore 
		SPPMath
		SPPSceneO
		)
		
target_include_directories(SPPAnimation 
	PRIVATE  			
		"${THIRD_PARTY}/ozz-animation/include" )
		
target_link_directories(SPPAnimation 
	PRIVATE
		"${THIRD_PARTY}/ozz-animation/lib" )

list(APPEND 
		OZZDepends 
			"ozz_base"
			"ozz_animation"
			"ozz_animation_offline" )

set(OZZDepends_D "")
set(OZZDepends_R "")

set(OZZdelayLoads_R "")
set(OZZdelayLoads_D "")

foreach(curDepend ${OZZDepends})
	list(APPEND OZZDepends_D "${curDepend}_d")
	list(APPEND OZZDepends_R "${curDepend}_r")
	
	set(OZZdelayLoads_R "${OZZdelayLoads_R} /DELAYLOAD:${curDepend}_r.dll")
	set(OZZdelayLoads_D "${OZZdelayLoads_D} /DELAYLOAD:${curDepend}_d.dll")
endforeach()
		
target_link_libraries(SPPAnimation 
	PRIVATE
		debug "${OZZDepends_D}" optimized "${OZZDepends_R}" )

if(OS_WINDOWS)
	set_target_properties(SPPAnimation PROPERTIES LINK_FLAGS_RELWITHDEBINFO "${OZZdelayLoads_R}" )
	set_target_properties(SPPAnimation PROPERTIES LINK_FLAGS_RELEASE "${OZZdelayLoads_R}" )
	set_target_properties(SPPAnimation PROPERTIES LINK_FLAGS_DEBUG "${OZZdelayLoads_D}" )
endif() 

install(TARGETS SPPAnimation DESTINATION "${SPP_ROOT}/INSTALL")
