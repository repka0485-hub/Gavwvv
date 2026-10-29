cmake_minimum_required(VERSION 3.18)
project(chams CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(GLOB DOBBY_LIBS ${CMAKE_SOURCE_DIR}/dobby/arm64-v8a/*.a)

add_library(chams SHARED chams.cpp)
target_include_directories(chams PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/dobby/include)
target_link_libraries(chams PRIVATE
    -Wl,--start-group ${DOBBY_LIBS} -Wl,--end-group
    EGL GLESv3 log)
