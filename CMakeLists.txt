cmake_minimum_required(VERSION 3.10)
project(rist-cpp)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(WIN32)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
find_package(GTest REQUIRED)

#If no build type is set then force Release
IF( NOT CMAKE_BUILD_TYPE )
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
            "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
            FORCE)
ENDIF()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package (Threads REQUIRED)
include(ExternalProject)

if(WIN32)
set(BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/rist/build")
set(BUILD_CMD cd ${CMAKE_CURRENT_SOURCE_DIR}/rist/build && meson -Dbuiltin_cjson=true --default-library=static .. && ninja)
ExternalProject_Add(project_rist
        CONFIGURE_COMMAND ""
        GIT_REPOSITORY https://code.videolan.org/rist/librist.git
        GIT_TAG v0.2.10
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rist
        BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rist
        BUILD_COMMAND ${BUILD_CMD}
        GIT_PROGRESS 1
        STEP_TARGETS build
        EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/rist/build/librist.a
        )
else()
ExternalProject_Add(project_rist
        CONFIGURE_COMMAND ""
        GIT_REPOSITORY https://code.videolan.org/rist/librist.git
        GIT_TAG v0.2.10
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rist
        BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rist
        BUILD_COMMAND mkdir -p ${CMAKE_CURRENT_SOURCE_DIR}/rist/build COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/rist/build COMMAND meson -Dbuiltin_cjson=true --default-library=static .. COMMAND ninja
        GIT_PROGRESS 1
        STEP_TARGETS build
        EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/rist/build/librist.a
        )
endif()

add_library(rist STATIC IMPORTED)
set_property(TARGET rist PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/rist/build/librist.a)
add_dependencies(rist project_rist)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rist/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rist/include/librist)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rist/build/include/librist)

set_source_files_properties( ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/lz4.c PROPERTIES GENERATED TRUE)
set_source_files_properties( ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/lz4frame.c PROPERTIES GENERATED TRUE)
set_source_files_properties( ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/lz4hc.c PROPERTIES GENERATED TRUE)
set_source_files_properties( ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/xxhash.c PROPERTIES GENERATED TRUE)

add_library(ristnet STATIC
        RISTNet.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/lz4.c
        ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/lz4frame.c
        ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/lz4hc.c
        ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/xxhash.c
        )
target_link_libraries(ristnet rist Threads::Threads)

add_executable(rist_cpp main.cpp)
target_link_libraries(rist_cpp ristnet)

#
# Build unit tests using GoogleTest
#
if(UNIX)
	enable_testing()

	add_executable(runUnitTests
			${CMAKE_CURRENT_SOURCE_DIR}/test/TestRist.cpp
	)
	target_compile_options(runUnitTests PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-unused-function)

	target_include_directories(runUnitTests
			PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
			PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test)

	target_link_libraries(runUnitTests ristnet GTest::GTest GTest::Main)
endif()
