cmake_minimum_required(VERSION 3.15)
project(rtbot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(lib)

#---------- external dependencies ---------

include(external/external.cmake)

#----------- tests -----------------

add_subdirectory(test)

#---------- pybinding ---------------

add_subdirectory(wrappers/python)

#----------- example -----------------

#add_subdirectory(example/c++)
