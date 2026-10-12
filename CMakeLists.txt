# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.13)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Install after the toolchain loads IDF utilities, before components are expanded.
set(CMAKE_PROJECT_esp32-game_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedData.cmake")

project(esp32-game)
