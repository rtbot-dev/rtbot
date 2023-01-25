workspace(name = "rtbot")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

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
    build_file = "@//libs/core/external:yaml-cpp.BUILD",
    sha256 = "4d5e664a7fb2d7445fc548cc8c0e1aa7b1a496540eb382d137e2cc263e6d3ef5",
    strip_prefix = "yaml-cpp-yaml-cpp-0.7.0",
    url = "https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.7.0.zip",
)

http_archive(
    name = "json-cpp",
    build_file = "@//libs/core/external:json-cpp.BUILD",
    sha256 = "95651d7d1fcf2e5c3163c3d37df6d6b3e9e5027299e6bd050d157322ceda9ac9",
    strip_prefix = "json-3.11.2",
    url = "https://github.com/nlohmann/json/archive/refs/tags/v3.11.2.zip",
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

# We still require the pybind library.
http_archive(
    name = "pybind11",
    build_file = "@pybind11_bazel//:pybind11.BUILD",
    strip_prefix = "pybind11-2.10.2",
    urls = ["https://github.com/pybind/pybind11/archive/v2.10.2.tar.gz"],
)

load("@pybind11_bazel//:python_configure.bzl", "python_configure")

python_configure(
    name = "local_config_python",
    python_version = "3",
)

# node configuration

http_archive(
    name = "aspect_rules_js",
    sha256 = "c4a5766a45dff25b2eb1789d7a2decfda81b281fc88350d24687620c37fefb25",
    strip_prefix = "rules_js-1.14.0",
    url = "https://github.com/aspect-build/rules_js/archive/refs/tags/v1.14.0.tar.gz",
)

http_archive(
    name = "aspect_rules_swc",
    sha256 = "71bff4030067e3898a98a60918745a168b166256393d1ea566f72b118460d4ef",
    strip_prefix = "rules_swc-0.21.0",
    url = "https://github.com/aspect-build/rules_swc/archive/refs/tags/v0.21.0.tar.gz",
)

http_archive(
    name = "aspect_rules_ts",
    sha256 = "e81f37c4fe014fc83229e619360d51bfd6cb8ac405a7e8018b4a362efa79d000",
    strip_prefix = "rules_ts-1.0.4",
    url = "https://github.com/aspect-build/rules_ts/archive/refs/tags/v1.0.4.tar.gz",
)

load("@aspect_rules_js//js:repositories.bzl", "rules_js_dependencies")

rules_js_dependencies()

load("@aspect_rules_ts//ts:repositories.bzl", "rules_ts_dependencies")

rules_ts_dependencies(ts_version_from = "//:package.json")

load("@rules_nodejs//nodejs:repositories.bzl", "DEFAULT_NODE_VERSION", "nodejs_register_toolchains")

nodejs_register_toolchains(
    name = "nodejs",
    node_version = DEFAULT_NODE_VERSION,
)

load("@aspect_rules_js//npm:npm_import.bzl", "npm_translate_lock")

npm_translate_lock(
    name = "npm",
    pnpm_lock = "//:pnpm-lock.yaml",
    verify_node_modules_ignored = "//:.bazelignore",
)

load("@npm//:repositories.bzl", "npm_repositories")

npm_repositories()

load("@aspect_rules_swc//swc:dependencies.bzl", "rules_swc_dependencies")

rules_swc_dependencies()

load("@aspect_rules_swc//swc:repositories.bzl", "swc_register_toolchains", LATEST_SWC_VERSION = "LATEST_VERSION")

swc_register_toolchains(
    name = "swc",
    swc_version = LATEST_SWC_VERSION,
)

load("//:tools/prisma/prisma.bzl", "prisma_generate")

prisma_generate(
    name = "postgres-client",
    schema = "//:libs/postgres/prisma/schema.prisma",
)
