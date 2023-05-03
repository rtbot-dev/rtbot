add_executable(test-api
    tools.h
    test_pipeline.cpp
    ppg.json
)


target_include_directories(test-api INTERFACE Catch2::Catch2)
target_link_libraries(test-api PRIVATE ${PROJECT_NAME}-api)
target_link_libraries(test-api PRIVATE Catch2::Catch2)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/contrib/)
include(CTest)
include(Catch)
catch_discover_tests(test-api)


target_include_directories(test-api INTERFACE nlohmann_json::nlohmann_json)
target_link_libraries(test-api PRIVATE nlohmann_json::nlohmann_json)
