
cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

project(i8080-emulator LANGUAGES C)

option(BUILD_TEST_EMULATOR "Build i8080-emulator to run test files" OFF)
option(BUILD_SPACE_INVADERS_EMULATOR "Build the space invaders emulator" OFF)

add_library(i8080 STATIC i8080.c)
target_include_directories(i8080 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (BUILD_TEST_EMULATOR)
  add_executable(i8080-emulator)
  target_sources(i8080-emulator PRIVATE i8080-emulator.c)
  target_link_libraries(i8080-emulator PRIVATE i8080)
endif ()

if (BUILD_SPACE_INVADERS_EMULATOR)
  find_package(SDL2)
  if (NOT SDL2_FOUND)
    message("SDL2 not found")
  else ()
    add_executable(space-invaders)
    target_sources(space-invaders PRIVATE space-invaders.c)
    target_link_libraries(space-invaders PRIVATE i8080 SDL2)
  endif ()
endif ()

