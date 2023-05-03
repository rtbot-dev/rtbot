add_executable(test-std
    test_join.cpp
    test_peak_detector.cpp
    test_resamplers.cpp
    tools.h
)

target_include_directories(test-std INTERFACE Catch2::Catch2)
target_link_libraries(test-std PRIVATE ${PROJECT_NAME}-std)
target_link_libraries(test-std PRIVATE Catch2::Catch2)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/contrib/)
include(CTest)
include(Catch)
catch_discover_tests(test-std)


target_include_directories(test-std INTERFACE nlohmann_json::nlohmann_json)
target_link_libraries(test-std PRIVATE nlohmann_json::nlohmann_json)
