
cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
project(i8080-emulator LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Intel 8080 emulator library.
add_library(i8080)
target_sources(i8080 PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/i8080.c
  ${CMAKE_CURRENT_LIST_DIR}/i8080.h
)
target_include_directories(i8080 PUBLIC ${CMAKE_CURRENT_LIST_DIR})

# Emulator to run test roms.
add_executable(i8080-emulator)
target_sources(i8080-emulator PRIVATE i8080-emulator.c)
target_link_libraries(i8080-emulator PRIVATE i8080)

# Build the Space Invaders emulator if SDL2 can be found.
find_package(SDL2)
if (NOT SDL2_FOUND)
  message(WARNING "Cannot find SDL2, not building 'space-invaders'.")
else ()
  add_executable(space-invaders)
  target_sources(space-invaders PRIVATE space-invaders.c)
  target_include_directories(space-invaders PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(space-invaders PRIVATE i8080 ${SDL2_LIBRARIES})
endif ()
