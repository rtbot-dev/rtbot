set(header
    include/rtbot/Buffer.h
    include/rtbot/Group.h
    include/rtbot/Operator.h
)

add_library(${PROJECT_NAME} STATIC
    src/empty.cpp
    ${header}
)


