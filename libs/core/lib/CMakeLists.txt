set(header
    include/rtbot/Buffer.h
    include/rtbot/Group.h
    include/rtbot/Operator.h
)

add_library(${PROJECT_NAME} STATIC
    src/empty.cpp
    ${header}
)


set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
