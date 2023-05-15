cmake_minimum_required(VERSION 3.15)
project(rtbot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(core/lib)
add_subdirectory(core/std)
add_subdirectory(core/api)

#---------- external dependencies ---------

include(external/external.cmake)

#----------- tests -----------------

add_subdirectory(core/lib/test)
add_subdirectory(core/std/test)
add_subdirectory(core/api/test)

#---------- pybinding ---------------

add_subdirectory(core/wrappers/python)

#----------- example -----------------

#add_subdirectory(example/c++)
