cmake_minimum_required(VERSION 3.4.1)

add_library(hideosksuggestions SHARED
            hideosksuggestions.cpp)

# Include libraries needed for hideosksuggestions lib
target_link_libraries(hideosksuggestions
                      android
                      log)
