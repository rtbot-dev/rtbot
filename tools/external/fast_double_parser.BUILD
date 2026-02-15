cc_library(
    name = "lib",
    visibility = ["//visibility:public"],
    includes = ["include"],
    hdrs = glob(["include/**/*.h", "include/**/*.hpp"], allow_empty = True),
    srcs = glob(["src/**/*.cpp"], allow_empty = True),
    copts = [
        "-Iexternal/fast_double_parser/include",
    ],
)
