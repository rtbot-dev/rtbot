pybind11_add_module(${PROJECT_NAME}api
    rtbot.cpp
)

target_link_libraries(${PROJECT_NAME}api PUBLIC ${PROJECT_NAME}-api)
target_link_libraries(${PROJECT_NAME}api PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_NAME}api PRIVATE nlohmann_json_schema_validator::validator)

