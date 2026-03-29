# connections 

cmake_minimum_required(VERSION 3.16)
project(connections LANGUAGES CXX)

# Set lib dir to lib
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../lib")

aux_source_directory(. sources)
add_library(connections STATIC ${sources})
set_target_properties(connections PROPERTIES DEBUG_POSTFIX "d")
