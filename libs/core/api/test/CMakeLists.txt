file(GLOB test_api_SRC CONFIGURE_DEPENDS "*.h" "*.cpp")

add_executable(test-api ${test_api_SRC})

target_link_libraries(test-api PRIVATE ${PROJECT_NAME}-api)
target_link_libraries(test-api PRIVATE Catch2::Catch2)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/contrib/)
include(CTest)
include(Catch)
catch_discover_tests(test-api)


target_link_libraries(test-api PRIVATE nlohmann_json::nlohmann_json)

target_link_libraries(test-api PRIVATE nlohmann_json_schema_validator)


file(COPY ${CMAKE_SOURCE_DIR}/../examples/data/ppg.csv
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/examples/data)

file(COPY ${CMAKE_SOURCE_DIR}/../examples/data/program-test-1.json
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/examples/data)
file(COPY ${CMAKE_SOURCE_DIR}/../examples/data/program-test-2.json
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/examples/data)
file(COPY ${CMAKE_SOURCE_DIR}/../examples/data/program-test-3.json
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/examples/data)
file(COPY ${CMAKE_SOURCE_DIR}/../examples/data/program-test-4.json
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/examples/data)
