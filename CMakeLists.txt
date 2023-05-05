
cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(i8080-emulator LANGUAGES C)
set(CMAKE_C_STANDARD 11)

find_package(SDL2 REQUIRED)

add_library(i8080 STATIC i8080.c)
target_include_directories(i8080 PUBLIC ${CMAKE_SOURCE_DIR})

add_executable(i8080-emulator)
target_sources(i8080-emulator PRIVATE i8080-emulator.c)
target_link_libraries(i8080-emulator PRIVATE i8080)

add_executable(spaceinvaders)
target_sources(spaceinvaders PRIVATE spaceinvaders.c)
target_link_libraries(spaceinvaders PRIVATE i8080 ${SDL2_LIBRARIES})
