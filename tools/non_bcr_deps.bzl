load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _non_bcr_deps_impl(module_ctx):
    http_archive(
        name = "catch2",
        sha256 = "121e7488912c2ce887bfe4699ebfb983d0f2e0d68bcd60434cdfd6bb0cf78b43",
        strip_prefix = "Catch2-2.13.10",
        urls = ["https://github.com/catchorg/Catch2/archive/v2.13.10.zip"],
    )

    http_archive(
        name = "json-cpp",
        build_file = "//tools/external:json-cpp.BUILD",
        sha256 = "95651d7d1fcf2e5c3163c3d37df6d6b3e9e5027299e6bd050d157322ceda9ac9",
        strip_prefix = "json-3.11.2",
        url = "https://github.com/nlohmann/json/archive/refs/tags/v3.11.2.zip",
    )

    http_archive(
        name = "yaml-cpp",
        build_file = "//tools/external:yaml-cpp.BUILD",
        sha256 = "4d5e664a7fb2d7445fc548cc8c0e1aa7b1a496540eb382d137e2cc263e6d3ef5",
        strip_prefix = "yaml-cpp-yaml-cpp-0.7.0",
        url = "https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.7.0.zip",
    )

    http_archive(
        name = "json-schema-validator",
        build_file = "//tools/external:json-schema-validator.BUILD",
        sha256 = "d6dd8415b88c70faac6e2a2f1c61cb65de839b332e4ec91a317de384c0edc01e",
        strip_prefix = "json-schema-validator-2.2.0",
        url = "https://github.com/pboettch/json-schema-validator/archive/refs/tags/2.2.0.zip",
    )

    http_archive(
        name = "fast_double_parser",
        build_file = "//tools/external:fast_double_parser.BUILD",
        sha256 = "fc408309a03dc1606620c8be358e98c652479766afd50e5cdb22032a3f09b5d8",
        strip_prefix = "fast_double_parser-0.7.0",
        url = "https://github.com/lemire/fast_double_parser/archive/refs/tags/v0.7.0.zip",
    )

    http_archive(
        name = "quill",
        build_file = "//tools/external:quill.BUILD",
        strip_prefix = "quill-6f71257bfd58b6dbfd26cb7ad5a2453bb9844bce/quill",
        url = "https://github.com/odygrd/quill/archive/6f71257bfd58b6dbfd26cb7ad5a2453bb9844bce.zip",
    )

    http_archive(
        name = "ftxui",
        build_file = "//tools/external:ftxui.BUILD",
        sha256 = "a2991cb222c944aee14397965d9f6b050245da849d8c5da7c72d112de2786b5b",
        strip_prefix = "FTXUI-5.0.0",
        url = "https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v5.0.0.tar.gz",
    )

    http_archive(
        name = "cxxopts",
        build_file = "//tools/external:cxxopts.BUILD",
        sha256 = "9f43fa972532e5df6c5fd5ad0f5bac606cdec541ccaf1732463d8070bbb7f03b",
        strip_prefix = "cxxopts-3.2.0",
        url = "https://github.com/jarro2783/cxxopts/archive/refs/tags/v3.2.0.tar.gz",
    )

    http_archive(
        name = "linenoise",
        build_file = "//tools/external:linenoise.BUILD",
        strip_prefix = "cpp-linenoise-master",
        url = "https://github.com/yhirose/cpp-linenoise/archive/refs/heads/master.zip",
    )

non_bcr_deps = module_extension(
    implementation = _non_bcr_deps_impl,
)
