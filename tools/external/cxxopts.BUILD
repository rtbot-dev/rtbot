cc_library(
    name = "lib",
    srcs = glob(
        [
            "src/**/*.cpp",
            "include/**/*.hpp",
        ],
        exclude = ["**/*_test.cpp"],
    ),
    hdrs = glob([
        "**/*.h",
        "**/*.hpp",
    ]),
    includes = [
        "include",
        "src",
    ],
    visibility = ["//visibility:public"],
)
