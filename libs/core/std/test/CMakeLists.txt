file(GLOB test_std_SRC CONFIGURE_DEPENDS "*.h" "*.cpp")

add_executable(test-std ${test_std_SRC})

target_link_libraries(test-std PRIVATE ${PROJECT_NAME}-std)
target_link_libraries(test-std PRIVATE Catch2::Catch2)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/contrib/)
include(CTest)
include(Catch)
catch_discover_tests(test-std)


target_link_libraries(test-std PRIVATE nlohmann_json::nlohmann_json)

file(COPY ${CMAKE_SOURCE_DIR}/../examples/data/ppg.csv
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/examples/data)
