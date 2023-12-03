file(GLOB api_SRC CONFIGURE_DEPENDS "include/rtbot/*.h" "src/*.cpp")

add_library(${PROJECT_NAME}-api STATIC ${api_SRC})

set_property(TARGET ${PROJECT_NAME}-api PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(${PROJECT_NAME}-api PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(${PROJECT_NAME}-api PUBLIC ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME}-api PUBLIC ${PROJECT_NAME}-std)
target_link_libraries(${PROJECT_NAME}-api PUBLIC ${PROJECT_NAME}-fin)
target_link_libraries(${PROJECT_NAME}-api PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_NAME}-api PRIVATE nlohmann_json_schema_validator::validator)

