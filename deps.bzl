load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def deps():
    http_archive(
        name = "bazel_skylib",
        sha256 = "74d544d96f4a5bb630d465ca8bbcfe231e3594e5aae57e1edbf17a6eb3ca2506",
        urls = ["https://github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz"],
    )

    http_archive(
        name = "yaml-cpp",
        build_file = "@rtbot//libs/external:yaml-cpp.BUILD",
        sha256 = "4d5e664a7fb2d7445fc548cc8c0e1aa7b1a496540eb382d137e2cc263e6d3ef5",
        strip_prefix = "yaml-cpp-yaml-cpp-0.7.0",
        url = "https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.7.0.zip",
    )

    http_archive(
        name = "json-cpp",
        build_file = "@rtbot//libs/external:json-cpp.BUILD",
        sha256 = "95651d7d1fcf2e5c3163c3d37df6d6b3e9e5027299e6bd050d157322ceda9ac9",
        strip_prefix = "json-3.11.2",
        url = "https://github.com/nlohmann/json/archive/refs/tags/v3.11.2.zip",
    )

    http_archive(
        name = "fast_double_parser",
        build_file = "@rtbot//libs/external:fast_double_parser.BUILD",
        sha256 = "fc408309a03dc1606620c8be358e98c652479766afd50e5cdb22032a3f09b5d8",
        strip_prefix = "fast_double_parser-0.7.0",
        url = "https://github.com/lemire/fast_double_parser/archive/refs/tags/v0.7.0.zip",
    )

    http_archive(
        name = "quill",
        build_file = "@rtbot//libs/external:quill.BUILD",
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
        sha256 = "2a88d837f8fb7bfe46b1d9f413df9a777ec2973e1f812929b597c1971a3a1da5",
        strip_prefix = "rules_js-1.28.0",
        url = "https://github.com/aspect-build/rules_js/releases/download/v1.28.0/rules_js-v1.28.0.tar.gz",
    )

    http_archive(
        name = "aspect_rules_js",
        sha256 = "0b69e0967f8eb61de60801d6c8654843076bf7ef7512894a692a47f86e84a5c2",
        strip_prefix = "rules_js-1.27.1",
        url = "https://github.com/aspect-build/rules_js/releases/download/v1.27.1/rules_js-v1.27.1.tar.gz",
    )

    http_archive(
        name = "aspect_rules_ts",
        sha256 = "40ab6d3d9cc3259da54fe2f162588aba92244af0f151fbc905dcc8e7b8744296",
        strip_prefix = "rules_ts-1.4.2",
        url = "https://github.com/aspect-build/rules_ts/releases/download/v1.4.2/rules_ts-v1.4.2.tar.gz",
    )

    http_archive(
        name = "aspect_rules_jest",
        sha256 = "175f92448bd11b398ee94c6bb09cabf76df75a77d21c9555723798c58a2e73c8",
        strip_prefix = "rules_jest-0.19.2",
        url = "https://github.com/aspect-build/rules_jest/releases/download/v0.19.2/rules_jest-v0.19.2.tar.gz",
    )

    http_archive(
        name = "aspect_rules_swc",
        sha256 = "84567d61fca690884c08adc44c9f72d0cc4b65dd6d705baf3e447a4d0a020856",
        strip_prefix = "rules_swc-1.0.0-rc2",
        url = "https://github.com/aspect-build/rules_swc/releases/download/v1.0.0-rc2/rules_swc-v1.0.0-rc2.tar.gz",
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
