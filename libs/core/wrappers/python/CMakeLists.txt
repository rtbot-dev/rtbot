pybind11_add_module(${PROJECT_NAME}py
    rtbot.cpp
)
target_link_libraries(${PROJECT_NAME}py PUBLIC ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME}py PUBLIC ${PROJECT_NAME}-std)
target_link_libraries(${PROJECT_NAME}py PUBLIC ${PROJECT_NAME}-api)
