file(GLOB fin_SRC CONFIGURE_DEPENDS "include/rtbot/finance/*.h" "src/*.cpp")

add_library(${PROJECT_NAME}-fin STATIC ${fin_SRC})

set_property(TARGET ${PROJECT_NAME}-fin PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(${PROJECT_NAME}-fin PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(${PROJECT_NAME}-fin PUBLIC ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME}-fin PUBLIC ${PROJECT_NAME}-std)
target_include_directories(${PROJECT_NAME}-fin INTERFACE nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_NAME}-fin PRIVATE nlohmann_json::nlohmann_json)

target_include_directories(${PROJECT_NAME}-fin INTERFACE nlohmann_json_schema_validator)
target_link_libraries(${PROJECT_NAME}-fin PRIVATE nlohmann_json_schema_validator)

