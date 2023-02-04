cc_library(
    name = "lib",
    visibility = ["//visibility:public"],
    includes = ["include"],
    hdrs = glob(["include/**/*.h"]),
    srcs = glob([
        "src/**/*.cpp",
        "src/**/*.cc",
    ]),
    copts = [
        "-Wall",
        "-Wextra",
        "-Wconversion",
        "-pedantic",
        "-Wfatal-errors",
        "-Wno-unused-private-field",
        "-Wno-gnu-zero-variadic-macro-arguments",
        "-Wno-unused-parameter",
    ],
)
