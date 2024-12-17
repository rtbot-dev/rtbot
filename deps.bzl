load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def deps():
    http_archive(
        name = "bazel_skylib",
        sha256 = "74d544d96f4a5bb630d465ca8bbcfe231e3594e5aae57e1edbf17a6eb3ca2506",
        urls = ["https://github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz"],
    )

    http_archive(
        name = "yaml-cpp",
        build_file = "@rtbot//tools/external:yaml-cpp.BUILD",
        sha256 = "4d5e664a7fb2d7445fc548cc8c0e1aa7b1a496540eb382d137e2cc263e6d3ef5",
        strip_prefix = "yaml-cpp-yaml-cpp-0.7.0",
        url = "https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.7.0.zip",
    )

    http_archive(
        name = "json-cpp",
        build_file = "@rtbot//tools/external:json-cpp.BUILD",
        sha256 = "95651d7d1fcf2e5c3163c3d37df6d6b3e9e5027299e6bd050d157322ceda9ac9",
        strip_prefix = "json-3.11.2",
        url = "https://github.com/nlohmann/json/archive/refs/tags/v3.11.2.zip",
    )

    http_archive(
        name = "json-schema-validator",
        build_file = "@rtbot//tools/external:json-schema-validator.BUILD",
        sha256 = "d6dd8415b88c70faac6e2a2f1c61cb65de839b332e4ec91a317de384c0edc01e",
        strip_prefix = "json-schema-validator-2.2.0",
        url = "https://github.com/pboettch/json-schema-validator/archive/refs/tags/2.2.0.zip",
    )

    http_archive(
        name = "fast_double_parser",
        build_file = "@rtbot//tools/external:fast_double_parser.BUILD",
        sha256 = "fc408309a03dc1606620c8be358e98c652479766afd50e5cdb22032a3f09b5d8",
        strip_prefix = "fast_double_parser-0.7.0",
        url = "https://github.com/lemire/fast_double_parser/archive/refs/tags/v0.7.0.zip",
    )

    http_archive(
        name = "quill",
        build_file = "@rtbot//tools/external:quill.BUILD",
        strip_prefix = "quill-6f71257bfd58b6dbfd26cb7ad5a2453bb9844bce/quill",
        url = "https://github.com/odygrd/quill/archive/6f71257bfd58b6dbfd26cb7ad5a2453bb9844bce.zip",
    )

    http_archive(
        name = "catch2",
        sha256 = "121e7488912c2ce887bfe4699ebfb983d0f2e0d68bcd60434cdfd6bb0cf78b43",
        strip_prefix = "Catch2-2.13.10",
        urls = ["https://github.com/catchorg/Catch2/archive/v2.13.10.zip"],
    )

    # needed by rules_python
    http_archive(
        name = "rules_cc",
        urls = ["https://github.com/bazelbuild/rules_cc/releases/download/0.0.17/rules_cc-0.0.17.tar.gz"],
        sha256 = "abc605dd850f813bb37004b77db20106a19311a96b2da1c92b789da529d28fe1",
        strip_prefix = "rules_cc-0.0.17",
    )

    http_archive(
        name = "rules_python",
        sha256 = "690e0141724abb568267e003c7b6d9a54925df40c275a870a4d934161dc9dd53",
        strip_prefix = "rules_python-0.40.0",
        url = "https://github.com/bazelbuild/rules_python/releases/download/0.40.0/rules_python-0.40.0.tar.gz",
    )

    http_archive(
        name = "pybind11_bazel",
        strip_prefix = "pybind11_bazel-2.13.6",
        urls = ["https://github.com/pybind/pybind11_bazel/archive/v2.13.6.zip"],
    )

    # We still require the pybind library.
    http_archive(
        name = "pybind11",
        build_file = "@pybind11_bazel//:pybind11-BUILD.bazel",
        strip_prefix = "pybind11-2.13.6",
        urls = ["https://github.com/pybind/pybind11/archive/v2.13.6.zip"],
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
        sha256 = "97f21c32c0c1eecb963d19d1cacf58538086ce28eb28274993ab21d3673b5c29",
        strip_prefix = "emsdk-3.1.68/bazel",
        url = "https://github.com/emscripten-core/emsdk/archive/refs/tags/3.1.68.tar.gz",
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

    http_archive(
        name = "benchmark",
        url = "https://github.com/google/benchmark/archive/refs/tags/v1.7.1.zip",
        sha256 = "aeec52381284ec3752505a220d36096954c869da4573c2e1df3642d2f0a4aac6",
        strip_prefix = "benchmark-1.7.1",
    )

    http_archive(
        name = "build_bazel_apple_support",
        sha256 = "b53f6491e742549f13866628ddffcc75d1f3b2d6987dc4f14a16b242113c890b",
        url = "https://github.com/bazelbuild/apple_support/releases/download/1.17.1/apple_support.1.17.1.tar.gz",
    )

    http_archive(
        name = "com_google_protobuf",
        # notice that when we change the version here we also have to change the version as well for
        # the buf plugin buf.build/protocolbuffers/cpp in the files:
        #  - libs/api/proto/buf.gen.yaml
        url = "https://github.com/protocolbuffers/protobuf/releases/download/v27.3/protobuf-27.3.zip",
        strip_prefix = "protobuf-27.3",
        sha256 = "a49147217f69e8d19aab0cc5c0059d6201261f5cb62145f8ab4ac8b94e7ffa86",
    )

    http_archive(
        name = "rules_buf",
        integrity = "sha256-Hr64Q/CaYr0E3ptAjEOgdZd1yc+cBjp7OG1wzuf3DIs=",
        strip_prefix = "rules_buf-0.3.0",
        urls = [
            "https://github.com/bufbuild/rules_buf/archive/refs/tags/v0.3.0.zip",
        ],
    )

    http_archive(
        name = "rules_proto",
        sha256 = "6fb6767d1bef535310547e03247f7518b03487740c11b6c6adb7952033fe1295",
        strip_prefix = "rules_proto-6.0.2",
        url = "https://github.com/bazelbuild/rules_proto/releases/download/6.0.2/rules_proto-6.0.2.tar.gz",
    )

    http_archive(
        name = "protovalidate",
        sha256 = "6e49ffdf4502d32472b568cf1b279d113cc8548ffb19134f800f903d5148184b",
        url = "https://github.com/bufbuild/protovalidate/archive/refs/tags/v0.8.2.zip",
        strip_prefix = "protovalidate-0.8.2",
    )

    http_archive(
        name = "io_opentelemetry_cpp",
        sha256 = "b149109d5983cf8290d614654a878899a68b0c8902b64c934d06f47cd50ffe2e",
        strip_prefix = "opentelemetry-cpp-1.18.0",
        url = "https://github.com/open-telemetry/opentelemetry-cpp/archive/refs/tags/v1.18.0.tar.gz",
    )

    http_archive(
        name = "ftxui",
        strip_prefix = "FTXUI-5.0.0",
        sha256 = "a2991cb222c944aee14397965d9f6b050245da849d8c5da7c72d112de2786b5b",
        build_file = "@rtbot//tools/external:ftxui.BUILD",
        url = "https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v5.0.0.tar.gz",
    )

    http_archive(
        name = "cxxopts",
        strip_prefix = "cxxopts-3.2.0",
        sha256 = "9f43fa972532e5df6c5fd5ad0f5bac606cdec541ccaf1732463d8070bbb7f03b",
        build_file = "@rtbot//tools/external:cxxopts.BUILD",
        url = "https://github.com/jarro2783/cxxopts/archive/refs/tags/v3.2.0.tar.gz",
    )

    http_archive(
        name = "linenoise",
        strip_prefix = "cpp-linenoise-master",
        build_file = "@rtbot//tools/external:linenoise.BUILD",
        url = "https://github.com/yhirose/cpp-linenoise/archive/refs/heads/master.zip",
    )

    http_archive(
        name = "lua",
        build_file = "@rtbot//tools/external:lua.BUILD",
        sha256 = "5c39111b3fc4c1c9e56671008955a1730f54a15b95e1f1bd0752b868b929d8e3",
        strip_prefix = "lua-5.4.7",
        urls = ["https://github.com/lua/lua/archive/refs/tags/v5.4.7.tar.gz"],
    )

    http_archive(
        name = "sol2",
        build_file = "@rtbot//tools/external:sol2.BUILD",
        sha256 = "b82c5de030e18cb2bcbcefcd5f45afd526920c517a96413f0b59b4332d752a1e",
        strip_prefix = "sol2-3.3.0",
        urls = ["https://github.com/ThePhD/sol2/archive/v3.3.0.tar.gz"],
    )
