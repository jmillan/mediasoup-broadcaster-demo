cmake_minimum_required(VERSION 3.8)

project(broadcaster LANGUAGES CXX)

# Set version number.
set(broadcaster_VERSION_MAJOR 0)
set(broadcaster_VERSION_MINOR 1)

# C++ standard requirements.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -v -Wall -stdlib=libc++")		# v:显示详细信息  Wall:启用所有编译器警告
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")						# debug开启调试, cmake -DCMAKE_BUILD_TYPE=Debug /path/to/source
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g")				# release 

message("\n=========== mediasoup-broadcaster-demo Build Configuration ===========\n")
message(STATUS "LIBWEBRTC_INCLUDE_PATH : " ${LIBWEBRTC_INCLUDE_PATH})
message(STATUS "LIBWEBRTC_BINARY_PATH  : " ${LIBWEBRTC_BINARY_PATH})
message(STATUS "OPENSSL_INCLUDE_DIR    : " ${OPENSSL_INCLUDE_DIR})
message(STATUS "CMAKE_USE_OPENSSL      : " ${CMAKE_USE_OPENSSL})
message("")

# Add some compile flags to our source files.
set_source_files_properties(${SOURCE_FILES}
	PROPERTIES COMPILE_FLAGS -Wall -Wextra -Wpedantic)

# Create target.
add_executable(${PROJECT_NAME} ${SOURCES})

# Extra libs required in order to use Apple media devices.
if(APPLE)
	find_library(APPLICATION_SERVICES ApplicationServices)
	find_library(AUDIO_TOOLBOX AudioToolbox)
	find_library(AV_FOUNDATION AVFoundation)
	find_library(CORE_AUDIO CoreAudio)
	find_library(CORE_FOUNDATION Foundation)
	find_library(CORE_MEDIA CoreMedia)
	find_library(CORE_VIDEO CoreVideo)
	find_library(CORE_SERVICES CoreServices)

	target_link_libraries(${PROJECT_NAME} PUBLIC
		${APPLICATION_SERVICES}
		${AUDIO_TOOLBOX}
		${AV_FOUNDATION}
		${CORE_AUDIO}
		${CORE_FOUNDATION}
		${CORE_MEDIA}
		${CORE_VIDEO}
		${CORE_SERVICES}
	)
endif(APPLE)

include(FetchContent)

message(STATUS "\nFetching mediasoupclient...\n")
FetchContent_Declare(
  mediasoupclient
  GIT_REPOSITORY git@github.com:janreyho/libmediasoupclient.git
  GIT_TAG m120
)
FetchContent_MakeAvailable(mediasoupclient)

message(STATUS "Fetching cpr...\n")
FetchContent_Declare(
	cpr
	GIT_REPOSITORY https://github.com/whoshuu/cpr
	GIT_TAG 1.4.0
)
FetchContent_MakeAvailable(cpr)

target_sources(${PROJECT_NAME} PRIVATE
	src/Broadcaster.cpp
	src/main.cpp
	src/MediaStreamTrackFactory.cpp
)

# Private (implementation) header files.
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Source Dependencies.
add_subdirectory(deps/libwebrtc "${CMAKE_CURRENT_BINARY_DIR}/libwebrtc")

# Public (interface) headers from dependencies.
target_include_directories(${PROJECT_NAME} PUBLIC
	${mediasoupclient_SOURCE_DIR}/include
	${cpr_SOURCE_DIR}/include
	"${PROJECT_SOURCE_DIR}/deps/libwebrtc"
)

# Public (interface) dependencies.
target_link_libraries(${PROJECT_NAME} PUBLIC
	${LIBWEBRTC_BINARY_PATH}/libwebrtc${CMAKE_STATIC_LIBRARY_SUFFIX}
	cpr
	mediasoupclient
	webrtc_broadcaster
)

