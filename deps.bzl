load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_jar")

def deps():
    # Apr 15, 2022
    ANTLR4_VERSION = "4.10.1"

    http_archive(
        name = "antlr4_runtimes",
        build_file = "@rtbot//libs/external:antlr4-runtimes.BUILD",
        sha256 = "a320568b738e42735946bebc5d9d333170e14a251c5734e8b852ad1502efa8a2",
        strip_prefix = "antlr4-" + ANTLR4_VERSION,
        urls = ["https://github.com/antlr/antlr4/archive/v" + ANTLR4_VERSION + ".tar.gz"],
    )

    http_jar(
        name = "antlr4_jar",
        urls = ["https://www.antlr.org/download/antlr-" + ANTLR4_VERSION + "-complete.jar"],
        sha256 = "41949d41f20d31d5b8277187735dd755108df52b38db6c865108d3382040f918",
    )

    http_archive(
        name = "cli11",
        build_file = "@rtbot//libs/external:cli11.BUILD",
        strip_prefix = "CLI11-2.3.2",
        url = "https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.3.2.zip",
    )

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
        name = "json-schema-validator",
        build_file = "@rtbot//libs/external:json-schema-validator.BUILD",
        sha256 = "d6dd8415b88c70faac6e2a2f1c61cb65de839b332e4ec91a317de384c0edc01e",
        strip_prefix = "json-schema-validator-2.2.0",
        url = "https://github.com/pboettch/json-schema-validator/archive/refs/tags/2.2.0.zip",
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
        name = "rules_python",
        sha256 = "0a8003b044294d7840ac7d9d73eef05d6ceb682d7516781a4ec62eeb34702578",
        strip_prefix = "rules_python-0.24.0",
        url = "https://github.com/bazelbuild/rules_python/releases/download/0.24.0/rules_python-0.24.0.tar.gz",
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
        sha256 = "17187c15710ac133a656c35c2f768f3cfa925888d26ed93a9fe9fde69a860aab",
        strip_prefix = "rules_swc-1.0.2",
        url = "https://github.com/aspect-build/rules_swc/releases/download/v1.0.2/rules_swc-v1.0.2.tar.gz",
    )

    http_archive(
        name = "aspect_rules_esbuild",
        sha256 = "098e38e5ee868c14a6484ba263b79e57d48afacfc361ba30137c757a9c4716d6",
        strip_prefix = "rules_esbuild-0.15.0",
        url = "https://github.com/aspect-build/rules_esbuild/releases/download/v0.15.0/rules_esbuild-v0.15.0.tar.gz",
    )

    http_archive(
        name = "aspect_bazel_lib",
        sha256 = "44f4f6d1ea1fc5a79ed6ca83f875038fee0a0c47db4f9c9beed097e56f8fad03",
        strip_prefix = "bazel-lib-1.34.0",
        url = "https://github.com/aspect-build/bazel-lib/releases/download/v1.34.0/bazel-lib-v1.34.0.tar.gz",
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
