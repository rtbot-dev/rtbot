cc_library(
    name = "lib",
    visibility = ["//visibility:public"],
    includes = ["include"],
    hdrs = glob(["include/**/*.h", "include/**/*.hpp"]),
    srcs = glob(["src/**/*.cpp"]),
    copts = [
        "-Iexternal/json-cpp/include",
    ],
)
