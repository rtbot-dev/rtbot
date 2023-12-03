file(GLOB test_fin_SRC CONFIGURE_DEPENDS "*.h" "*.cpp")

add_executable(test-fin ${test_fin_SRC})

target_link_libraries(test-fin PRIVATE ${PROJECT_NAME}-fin)
target_link_libraries(test-fin PRIVATE Catch2::Catch2)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/contrib/)
include(CTest)
include(Catch)
catch_discover_tests(test-fin)

target_link_libraries(test-fin PRIVATE nlohmann_json::nlohmann_json)
