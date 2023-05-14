file(GLOB std_SRC CONFIGURE_DEPENDS "include/rtbot/std/*.h" "src/*.cpp")

add_library(${PROJECT_NAME}-std STATIC ${std_SRC})


target_include_directories(${PROJECT_NAME}-std PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)


target_link_libraries(${PROJECT_NAME}-std PUBLIC ${PROJECT_NAME})
