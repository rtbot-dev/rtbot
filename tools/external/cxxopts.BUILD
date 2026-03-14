cc_library(
    name = "lib",
    srcs = glob(
        [
            "src/**/*.cpp",
            "include/**/*.hpp",
        ],
        allow_empty = True,
        exclude = ["**/*_test.cpp"],
    ),
    hdrs = glob([
        "**/*.h",
        "**/*.hpp",
    ], allow_empty = True),
    includes = [
        "include",
        "src",
    ],
    visibility = ["//visibility:public"],
)
