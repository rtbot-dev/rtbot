set(header
    include/rtbot/std/Composite.h
    include/rtbot/std/CosineResampler.h
    include/rtbot/std/FilterByValue.h
    include/rtbot/std/HermiteResampler.h
    include/rtbot/std/MovingAverage.h
    include/rtbot/std/PeakDetector.h
)

set(source
    src/empty.cpp
)

add_library(${PROJECT_NAME}-std STATIC
    ${header}
    ${source}
)


target_include_directories(${PROJECT_NAME}-std PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)


target_link_libraries(${PROJECT_NAME}-std PUBLIC ${PROJECT_NAME})
