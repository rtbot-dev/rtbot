yaml_cpp_defines = select({
    # On Windows, ensure static linking is used.
    "@platforms//os:windows": ["YAML_CPP_STATIC_DEFINE", "YAML_CPP_NO_CONTRIB"],
    "//conditions:default": [],
})

cc_library(
    name = "lib",
    visibility = ["//visibility:public"],
    includes = ["include"],
    hdrs = glob(["include/**/*.h"]),
    srcs = glob(["src/**/*.cpp", "src/**/*.h"]),
    copts = [
        "-Iexternal/yaml-cpp/include",
        "-fexceptions",
    ],
    defines = yaml_cpp_defines,
)
