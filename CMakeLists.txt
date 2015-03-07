cmake_minimum_required(VERSION 2.6)
project(AlterMeeya)

add_subdirectory(md5)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package(WindowsSDK)

if (CMAKE_BUILD_TYPE STREQUAL "")
  # CMake defaults to leaving CMAKE_BUILD_TYPE empty. This screws up
  # differentiation between debug and release builds.
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build, options are: None (CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif ()

set(AM_SOURCES
    altermeeya.cpp
    stdafx.cpp
    stdafx.h
    targetver.h
    profile.h
    profile.cpp
    )

include_directories(
    ${WINSDK_INCLUDE_DIR}
    )

add_definitions(-D_UNICODE -DUNICODE -DRPC_USE_NATIVE_WCHAR -D_SCL_SECURE_NO_WARNINGS)
add_executable(altermeeya WIN32 ${AM_SOURCES})
target_link_libraries(altermeeya md5)
