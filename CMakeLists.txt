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
        GIT_REPOSITORY https://code.videolan.org/rist/librist.git
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rist
        BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rist
        CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        BUILD_COMMAND cmake --build ${CMAKE_CURRENT_SOURCE_DIR}/rist --config ${CMAKE_BUILD_TYPE} --target rist
        GIT_PROGRESS 1
        STEP_TARGETS build
        EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        )

add_library(rist STATIC IMPORTED)
set_property(TARGET rist PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/rist/librist.a)
add_dependencies(rist project_rist)
#]]

#When developing librist then ->
#
#remove the existing rist directory then->
#${CMAKE_CURRENT_SOURCE_DIR} / mkdir rist
#git clone (repo) ./rist
#then enable the build block below and block comment the above build.
#[[
file(GLOB base "${CMAKE_CURRENT_SOURCE_DIR}/rist/src/*.c")
file(GLOB extra "${CMAKE_CURRENT_SOURCE_DIR}/rist/extra/*.c")
file(GLOB lz4 "${CMAKE_CURRENT_SOURCE_DIR}/rist/extra/lz4/*.c")
add_library(rist STATIC ${base} ${extra} ${lz4})
target_link_libraries(rist Threads::Threads)
]]

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rist/inc/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rist/src/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rist/extra/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rist/inc)

add_library(ristnet STATIC RISTNet.cpp)
target_link_libraries(ristnet rist Threads::Threads)

add_executable(rist_cpp main.cpp)
target_link_libraries(rist_cpp ristnet Threads::Threads)
