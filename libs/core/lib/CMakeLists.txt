set(header
    include/rtbot/Message.h
    include/rtbot/Buffer.h
    include/rtbot/Operator.h
    include/rtbot/FilterByValue.h
    include/rtbot/Output.h
    include/rtbot/Join.h
    include/rtbot/Composite.h
    include/rtbot/FactoryOp.h
    include/rtbot/Pipeline.h
    include/rtbot/bindings.h
    include/rtbot/Enums.h
    include/rtbot/tools/MovingAverage.h
    include/rtbot/tools/Autoregressive.h
    include/rtbot/tools/PeakDetector.h
    include/rtbot/tools/Finance.h
    include/rtbot/tools/Input.h
)

set(source
    src/empty.cpp
    src/Pipeline.cpp
    src/FactoryOp.cpp
    src/bindings.cpp
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

## dependence

target_include_directories(${PROJECT_NAME} INTERFACE nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
