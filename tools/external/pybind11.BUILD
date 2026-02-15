load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

exports_files(["LICENSE"])

OPTIONS = select({
    ":msvc_compiler": [],
    "//conditions:default": [
        "-fexceptions",
        "-Xclang-only=-Wno-undefined-inline",
        "-Xclang-only=-Wno-pragma-once-outside-header",
        "-Xgcc-only=-Wno-error",
    ],
})

INCLUDES = [
    "include/pybind11/**/*.h",
]

EXCLUDES = [
    "include/pybind11/common.h",
]

cc_library(
    name = "pybind11",
    hdrs = glob(
        INCLUDES,
        exclude = EXCLUDES,
    ),
    copts = OPTIONS,
    includes = ["include"],
)

config_setting(
    name = "msvc_compiler",
    flag_values = {"@bazel_tools//tools/cpp:compiler": "msvc-cl"},
)

config_setting(
    name = "osx",
    constraint_values = ["@platforms//os:osx"],
)
