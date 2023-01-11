_CMD = """
echo "{{
    \\\\"name\\\\": \\\\"@rtbot/{client_name}\\\\",
    \\\\"version\\\\": \\\\"0.1.0\\\\",
    \\\\"files\\\\": [\\\\"{client_path}\\\\"],
    \\\\"main\\\\": \\\\"{client_path}/index.js\\\\",
    \\\\"typings\\\\": \\\\"{client_path}/index.d.ts\\\\"
}}" > $@
    """

_PRISMA_BUILD = """
load("@aspect_rules_js//npm:defs.bzl", "npm_package")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "generated",
    srcs = glob([
        "{generated_dir}/**/*",
    ]),
    visibility = ["//visibility:public"]
)

genrule(
    name = 'package_json',
    outs = ['package.json'],
    cmd = \"\"\"{cmd}\"\"\"
)

npm_package(
    name = "js",
    srcs = [":package_json", ":generated"],
    package = "@rtbot/{client_name}",
    include_external_repositories = ["{client_name}"],
    visibility = ["//visibility:public"]
)
"""

def _prisma_generate_impl(repository_ctx):
    # copy prisma schema content to internal prisma/schema.prisma file
    schema_content = repository_ctx.read(repository_ctx.attr.schema)
    repository_ctx.file("schema.prisma", schema_content)

    repository_ctx.report_progress("Generating prisma client...")

    result = repository_ctx.execute([
        "npx",
        "prisma",
        "generate",
        "--schema=schema.prisma",
    ])

    repository_ctx.file("result-err.txt", result.stderr)
    repository_ctx.file("result.txt", result.stdout)

    # parse the output property of the schema
    output_path = schema_content.split("output")[1].split("=")[1].split("\n")[0].replace("\"", "").replace(" ", "").replace("./", "")

    generated_dir = output_path.split(repository_ctx.name)[0].replace("\\", "/").split("/")[0]
    repository_ctx.report_progress("Generating prisma client: done")
    cmd = _CMD.format(
        client_name = repository_ctx.attr.name,
        client_path = output_path,
    )
    repository_ctx.file("BUILD.bazel", _PRISMA_BUILD.format(
        client_name = repository_ctx.attr.name,
        cmd = cmd,
        generated_dir = generated_dir,
    ))

prisma_generate = repository_rule(
    implementation = _prisma_generate_impl,
    local = True,
    attrs = {
        "schema": attr.label(
            mandatory = True,
            allow_single_file = True,
        ),
    },
)
