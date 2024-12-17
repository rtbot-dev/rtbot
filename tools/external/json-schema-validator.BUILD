cc_library(
    name = "lib",
    srcs = glob(["src/**/*.cpp"]),  # we need this otherwise we get a linker error
    hdrs = glob(["src/**/*.hpp"]),
    # copts = [
    #     "-Iexternal/json-schema-validator/src",
    # ],
    includes = ["src"],
    visibility = ["//visibility:public"],
    deps = ["@json-cpp//:lib"],
)
