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
    copts = [
        "-Iftxui/include",
    ],
    includes = [
        "include",
        "src",
    ],
    visibility = ["//visibility:public"],
)
