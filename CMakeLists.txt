cmake_minimum_required(VERSION 3.15)

project(wand-demo)

file(REAL_PATH "/home/rock/src/effcc/sdk" EFF_SDK_ROOT_DIR)
include(${EFF_SDK_ROOT_DIR}/setup_sdk.cmake)
add_subdirectory(${EFF_SDK_ROOT_DIR} sdk)

add_subdirectory(wand)
