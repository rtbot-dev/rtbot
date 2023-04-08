load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def deps():
    http_archive(
        name = "bazel_skylib",
        sha256 = "74d544d96f4a5bb630d465ca8bbcfe231e3594e5aae57e1edbf17a6eb3ca2506",
        urls = ["https://github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz"],
    )

    http_archive(
        name = "aspect_bazel_lib",
        sha256 = "558d70e36425c317c70b19fb0f68241a3747bcf46561b5ffc19bed17527adbb3",
        strip_prefix = "bazel-lib-1.20.0",
        url = "https://github.com/aspect-build/bazel-lib/archive/refs/tags/v1.20.0.tar.gz",
    )

    http_archive(
        name = "yaml-cpp",
        build_file = "@rtbot//libs/core/external:yaml-cpp.BUILD",
        sha256 = "4d5e664a7fb2d7445fc548cc8c0e1aa7b1a496540eb382d137e2cc263e6d3ef5",
        strip_prefix = "yaml-cpp-yaml-cpp-0.7.0",
        url = "https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.7.0.zip",
    )

    http_archive(
        name = "json-cpp",
        build_file = "@rtbot//libs/core/external:json-cpp.BUILD",
        sha256 = "95651d7d1fcf2e5c3163c3d37df6d6b3e9e5027299e6bd050d157322ceda9ac9",
        strip_prefix = "json-3.11.2",
        url = "https://github.com/nlohmann/json/archive/refs/tags/v3.11.2.zip",
    )

    http_archive(
        name = "fast_double_parser",
        build_file = "@rtbot//libs/core/external:fast_double_parser.BUILD",
        sha256 = "fc408309a03dc1606620c8be358e98c652479766afd50e5cdb22032a3f09b5d8",
        strip_prefix = "fast_double_parser-0.7.0",
        url = "https://github.com/lemire/fast_double_parser/archive/refs/tags/v0.7.0.zip",
    )

    http_archive(
        name = "quill",
        build_file = "@rtbot//libs/core/external:quill.BUILD",
        strip_prefix = "quill-6f71257bfd58b6dbfd26cb7ad5a2453bb9844bce/quill",
        url = "https://github.com/odygrd/quill/archive/6f71257bfd58b6dbfd26cb7ad5a2453bb9844bce.zip",
    )

    http_archive(
        name = "catch2",
        sha256 = "121e7488912c2ce887bfe4699ebfb983d0f2e0d68bcd60434cdfd6bb0cf78b43",
        strip_prefix = "Catch2-2.13.10",
        urls = ["https://github.com/catchorg/Catch2/archive/v2.13.10.zip"],
    )

    http_archive(
        name = "pybind11_bazel",
        sha256 = "a185aa68c93b9f62c80fcb3aadc3c83c763854750dc3f38be1dadcb7be223837",
        strip_prefix = "pybind11_bazel-faf56fb3df11287f26dbc66fdedf60a2fc2c6631",
        urls = ["https://github.com/pybind/pybind11_bazel/archive/faf56fb3df11287f26dbc66fdedf60a2fc2c6631.zip"],
    )

    http_archive(
        name = "pybind11",
        build_file = "@pybind11_bazel//:pybind11.BUILD",
        strip_prefix = "pybind11-2.10.2",
        urls = ["https://github.com/pybind/pybind11/archive/v2.10.2.tar.gz"],
    )

    http_archive(
        name = "aspect_rules_js",
        sha256 = "c4a5766a45dff25b2eb1789d7a2decfda81b281fc88350d24687620c37fefb25",
        strip_prefix = "rules_js-1.14.0",
        url = "https://github.com/aspect-build/rules_js/archive/refs/tags/v1.14.0.tar.gz",
    )

    http_archive(
        name = "aspect_rules_ts",
        sha256 = "e81f37c4fe014fc83229e619360d51bfd6cb8ac405a7e8018b4a362efa79d000",
        strip_prefix = "rules_ts-1.0.4",
        url = "https://github.com/aspect-build/rules_ts/archive/refs/tags/v1.0.4.tar.gz",
    )

    http_archive(
        name = "aspect_rules_jest",
        sha256 = "b5bf5f740da458fc3199264be6ef7acb74ddbdc30b4a7f75503e5e164e6c1781",
        strip_prefix = "rules_jest-0.14.0",
        url = "https://github.com/aspect-build/rules_jest/archive/refs/tags/v0.14.0.tar.gz",
    )

    http_archive(
        name = "aspect_rules_swc",
        sha256 = "71bff4030067e3898a98a60918745a168b166256393d1ea566f72b118460d4ef",
        strip_prefix = "rules_swc-0.21.0",
        url = "https://github.com/aspect-build/rules_swc/archive/refs/tags/v0.21.0.tar.gz",
    )

    http_archive(
        name = "emsdk",
        strip_prefix = "emsdk-3.1.34/bazel",
        url = "https://github.com/emscripten-core/emsdk/archive/refs/tags/3.1.34.tar.gz",
    )

    http_archive(
        name = "rules_rust",
        sha256 = "d125fb75432dc3b20e9b5a19347b45ec607fabe75f98c6c4ba9badaab9c193ce",
        urls = ["https://github.com/bazelbuild/rules_rust/releases/download/0.17.0/rules_rust-v0.17.0.tar.gz"],
    )

    http_archive(
        name = "cxx.rs",
        sha256 = "80b340aa123475367ba8d084fb3da02ffa1218a1c0aed844e70e6bd9cda0e6ca",
        strip_prefix = "cxx-1.0.89",
        urls = ["https://github.com/dtolnay/cxx/archive/refs/tags/1.0.89.tar.gz"],
    )
