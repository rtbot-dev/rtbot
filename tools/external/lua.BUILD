cc_library(
    name = "lib",
    srcs = glob(
        ["**/*.c"],
        exclude = ["lua.c", "luac.c", "onelua.c"],
    ),
    hdrs = glob(["**/*.h"]),
    includes = ["."],
    visibility = ["//visibility:public"],
)
