cmake_minimum_required(VERSION 3.5)
project(SPPNetworking_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPNetworking SHARED "")

set_target_properties(SPPNetworking PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPNetworking SPPCore SPPDatabase )

# will need define to not create empty dll export
target_compile_definitions(SPPNetworking
	PRIVATE
		SPP_NETWORKING_EXPORT=1)
				
set(EXTRA_SOURCE "")

if(USE_LIBDATACHANNEL)
	set(EXTRA_SOURCE
		"${CMAKE_CURRENT_LIST_DIR}/SPPWebSockets.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPWebSockets.h")
endif()

target_sources(SPPNetworking
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPNetworking.h"
				
		"${CMAKE_CURRENT_LIST_DIR}/SPPSockets.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPSockets.h"		
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPNetworkConnection.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPNetworkConnection.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPNetworkMessenger.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPNetworkMessenger.h"
				
		"${CMAKE_CURRENT_LIST_DIR}/SPPNatTraversal.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPNatTraversal.h"
		
		"${CMAKE_CURRENT_LIST_DIR}/SPPNetworkConnection.h"
		
		"${EXTRA_SOURCE}"
		)

target_include_directories(SPPNetworking 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )

		
target_include_directories(SPPNetworking 
	PRIVATE  	
		"${THIRD_PARTY}/libjuice/include" )

target_link_directories(SPPNetworking 
	PRIVATE		
		"${THIRD_PARTY}/libjuice/lib" )
			
target_link_libraries(SPPNetworking 
	PUBLIC
		SPPCore
		SPPDatabase )

if(USE_CRYPTOCPP AND USE_CRYPTOCPP_NETCONN)
	target_compile_definitions(SPPNetworking
			PUBLIC
				SPP_NETCONN_CRYPTO=1)

	add_dependencies( SPPNetworking SPPCrypto )	

	target_link_libraries(SPPNetworking 
		PUBLIC
			SPPCrypto )
endif()

if(USE_LIBDATACHANNEL)
	target_compile_definitions(SPPNetworking
			PUBLIC
				SPP_LIBDATACHANNEL_CRYPTO=1
				RTC_ENABLE_WEBSOCKET=1 )

	target_include_directories(SPPNetworking 
	PRIVATE  	
		"${THIRD_PARTY}/libdatachannel/include" )

	target_link_directories(SPPNetworking 
		PRIVATE		
			"${THIRD_PARTY}/libdatachannel/lib" )
		
	target_link_libraries(SPPNetworking 
		PRIVATE
			debug datachanneld optimized datachannel )
endif()

if(OS_WINDOWS)

	target_link_libraries(SPPNetworking 
		PRIVATE
			"juice-static"
			"Bcrypt" )

else()

	target_link_libraries(SPPNetworking 
		PRIVATE
            debug juiced optimized juice )

endif()

install(TARGETS SPPNetworking DESTINATION "${SPP_ROOT}/INSTALL")

