add_executable(${PROJECT_NAME}_test
    test_input.cpp
)

#target_compile_definitions(${PROJECT_NAME}_test PRIVATE -DCATCH_CONFIG_MAIN)

target_link_libraries(${PROJECT_NAME}_test PRIVATE ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME}_test PRIVATE Catch2::Catch2)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/contrib/)
include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME}_test)

target_link_libraries(${PROJECT_NAME}_test PRIVATE nlohmann_json::nlohmann_json)
