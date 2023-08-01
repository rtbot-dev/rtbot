cc_library(
    name = "lib",
    visibility = ["//visibility:public"],
    includes = ["include"],
    srcs = glob(["src/**/*.cpp", "src/**/*.hpp"]),
    deps = ["@json-cpp//:lib"],
    copts = [
        "-Iexternal/json-schema-validator/src/",
    ],
)
