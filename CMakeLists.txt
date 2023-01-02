cmake_minimum_required(VERSION 3.15)
project(rtbot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(RTBOT_BUILD_TEST "Build the rtbot test suite" ON)
option(RTBOT_BUILD_PYTHON "Build the rtbot python library" ON)
option(RTBOT_BUILD_EXAMPLE "Build the rtbot examples" ON)

add_subdirectory(lib)

set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

#---------- external dependencies ---------

include(external/extern.cmake)

#----------- tests -----------------

if (RTBOT_BUILD_TEST)
    add_subdirectory(test)
endif()

#---------- pybinding ---------------

if (RTBOT_BUILD_PYTHON)
    add_subdirectory(python)
endif()

#----------- example -----------------

if (RTBOT_BUILD_EXAMPLE)
    add_subdirectory(example/c++)
endif()


