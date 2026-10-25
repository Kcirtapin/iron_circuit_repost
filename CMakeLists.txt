cmake_minimum_required(VERSION 3.5...3.30)
project(IronCircuit CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# All sources under src/ compiled into one library
file(GLOB_RECURSE MODEL_SOURCES "src/*.cpp")

add_library(iron_circuit_lib ${MODEL_SOURCES})
target_include_directories(iron_circuit_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_compile_features(iron_circuit_lib PUBLIC cxx_std_17)

# SDL2
find_package(SDL2 REQUIRED)
target_link_libraries(iron_circuit_lib PUBLIC SDL2::SDL2)

# Optional SDL2_image
find_package(SDL2_image QUIET)
if(SDL2_image_FOUND)
  target_link_libraries(iron_circuit_lib PUBLIC SDL2_image::SDL2_image)
  target_compile_definitions(iron_circuit_lib PUBLIC IRON_CIRCUIT_HAS_SDL_IMAGE=1)
endif()

# SDL2_ttf for UI text rendering
find_package(SDL2_ttf REQUIRED)
target_link_libraries(iron_circuit_lib PUBLIC SDL2_ttf::SDL2_ttf)

# Optional SDL2_mixer
find_package(SDL2_mixer QUIET)
if(SDL2_mixer_FOUND)
  target_link_libraries(iron_circuit_lib PUBLIC SDL2_mixer::SDL2_mixer)
  target_compile_definitions(iron_circuit_lib PUBLIC IRON_CIRCUIT_HAS_SDL_MIXER=1)
endif()

# Executables
add_executable(iron_circuit bin/main.cpp)
target_link_libraries(iron_circuit PRIVATE iron_circuit_lib)
target_include_directories(iron_circuit PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(level_maker bin/LevelMaker.cpp)
target_link_libraries(level_maker PRIVATE iron_circuit_lib)
target_include_directories(level_maker PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(level_test bin/test.cpp)
target_link_libraries(level_test PRIVATE iron_circuit_lib)
target_include_directories(level_test PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(audio_test bin/audio_test.cpp)
target_include_directories(audio_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(audio_test PRIVATE iron_circuit_lib)
# SDL2, SDL2_mixer, and their compile definitions are inherited transitively
# from iron_circuit_lib — no need to re-add them here