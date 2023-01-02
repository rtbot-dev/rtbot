find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
    endif()
endif()


if (RTBOT_BUILD_TEST)
    if (NOT EXISTS "${PROJECT_SOURCE_DIR}/external/Catch2/CMakeLists.txt")
        message(FATAL_ERROR "Please download git submodules and try again.")
    endif()
    add_subdirectory(external/Catch2)
endif()

if (RTBOT_BUILD_PYTHON)
    if (NOT EXISTS "${PROJECT_SOURCE_DIR}/external/pybind11/CMakeLists.txt")
        message(FATAL_ERROR "Please download git submodules and try again.")
    endif()
    add_subdirectory(external/pybind11)
endif()

if (RTBOT_BUILD_EXAMPLE)
    if (NOT EXISTS "${PROJECT_SOURCE_DIR}/external/yaml-cpp/CMakeLists.txt")
        message(FATAL_ERROR "Please download git submodules and try again.")
    endif()
    add_subdirectory(external/yaml-cpp)
endif()
