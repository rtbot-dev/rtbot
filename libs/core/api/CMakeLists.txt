set(header
    include/rtbot/bindings.h
    include/rtbot/FactoryOp.h
    include/rtbot/Pipeline.h
)

set(source
	src/bindings.cpp
	src/FactoryOp.cpp
	src/Pipeline.cpp
)

add_library(${PROJECT_NAME}-api STATIC
    ${header}
    ${source}
)


target_include_directories(${PROJECT_NAME}-api PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(${PROJECT_NAME}-api PUBLIC ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME}-api PUBLIC ${PROJECT_NAME}-std)
target_include_directories(${PROJECT_NAME}-api INTERFACE nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_NAME}-api PRIVATE nlohmann_json::nlohmann_json)
