cmake_minimum_required(VERSION 3.15)
project(rtbot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(core)
add_subdirectory(std)
add_subdirectory(finance)
add_subdirectory(api)

#---------- external dependencies ---------

include(../tools/external/external.cmake)

#----------- tests -----------------

add_subdirectory(core/test)
add_subdirectory(std/test)
add_subdirectory(finance/test)
add_subdirectory(api/test)

#---------- pybinding ---------------

#add_subdirectory(wrappers/python)

#----------- example -----------------

#add_subdirectory(example/c++)
