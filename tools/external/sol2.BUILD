cc_library(
    name = "lib",
    srcs = glob(["include/**/*.hpp"]),
    hdrs = glob(["**/*.h"]),
    includes = ["include"],
    visibility = ["//visibility:public"],
    deps = [
        "@lua//:lib",
    ],
)
