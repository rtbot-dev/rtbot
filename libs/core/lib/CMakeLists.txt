set(header
    include/rtbot/Message.h
    include/rtbot/Buffer.h
    include/rtbot/Operator.h
    include/rtbot/Output.h
    include/rtbot/Join.h
    include/rtbot/Enums.h
    include/rtbot/Input.h
)

set(source
    src/empty.cpp
)

add_library(${PROJECT_NAME} STATIC
    ${header}
    ${source}
)


set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
