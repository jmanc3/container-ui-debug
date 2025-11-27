cmake_minimum_required(VERSION 3.30)
project(containerdebug)

set(CMAKE_CXX_STANDARD 20)

add_executable(containerdebug main.cpp container.cpp events.cpp)


find_package(PkgConfig)
if (NOT PkgConfig_FOUND)
    message(FATAL_ERROR "You need to have pkg-config installed. On voidlinux, sudo xbps-install pkg-config")
endif ()


# first you add the subdirectory
add_subdirectory(raylib-5.5)
# to find the name 'tag' you should use, you grep for add_library in the taglib-1.13.1 folder
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)
# the following is so that you don't have to ../taglib-1.13.1/taglib/thing.h and can just use thing.h
#target_include_directories(${PROJECT_NAME} PRIVATE raylib-5.5)

# Checks if OSX and links appropriate frameworks (only required on MacOS)
if (APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework IOKit")
    target_link_libraries(${PROJECT_NAME} "-framework Cocoa")
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
endif()


