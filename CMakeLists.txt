cmake_minimum_required(VERSION 3.10)
project(rist-cpp)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

#If no build type is set then force Release
IF( NOT CMAKE_BUILD_TYPE )
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
            "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
            FORCE)
ENDIF()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

find_package (Threads REQUIRED)

##[[
include(ExternalProject)
ExternalProject_Add(project_rist
        CONFIGURE_COMMAND ""
        GIT_REPOSITORY https://code.videolan.org/rist/librist.git
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rist
        BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rist
        CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        BUILD_COMMAND mkdir -p ${CMAKE_CURRENT_SOURCE_DIR}/rist/build && cd ${CMAKE_CURRENT_SOURCE_DIR}/rist/build && meson .. --default-library=static && ninja
        GIT_PROGRESS 1
        STEP_TARGETS build
        EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        )

add_library(rist STATIC IMPORTED)
set_property(TARGET rist PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/rist/build/librist.a)
add_dependencies(rist project_rist)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rist/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rist/include/librist)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rist/build/include/librist)

file(GLOB lz4 "${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/*.c")
link_directories(${CMAKE_SOURCE_DIR}/rist/build/subprojects/mbedtls-2.23.0)
add_library(ristnet STATIC RISTNet.cpp ${lz4})
target_link_libraries(ristnet rist Threads::Threads cm_mbedtls cm_mbedcrypto cm_mbedx509)

add_executable(rist_cpp main.cpp)
target_link_libraries(rist_cpp ristnet Threads::Threads)
