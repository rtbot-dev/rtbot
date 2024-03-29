load("@aspect_rules_js//js:providers.bzl", "JsInfo")

def _rtbot_jsonschema_impl(ctx):
    """
    The "implementation function" for our rule.

    It starts with underscore, which means this function is private to this file.
    Args:
        ctx: Bazel's starlark execution context
    """
    target = ctx.attr.target

    if target == "jsonschema":
        genfiles = [ctx.actions.declare_file("%s/jsonschema.json" % ctx.label.name)]

    if target == "cpp":
        genfiles = [ctx.actions.declare_file("%s/jsonschema.hpp" % ctx.label.name)]

    if target == "python":
        genfiles = [ctx.actions.declare_file("%s/jsonschema.py" % ctx.label.name)]

    if target == "typescript":
        genfiles = [ctx.actions.declare_file("%s/index.ts" % ctx.label.name)]

    if target == "markdown":
        genfiles = []
        for f in ctx.files.srcs:
            genfiles.append(ctx.actions.declare_file("%s/%sx" % (ctx.label.name, f.basename)))

    output_dir = ctx.actions.declare_directory(ctx.label.name)

    args = ctx.actions.args()
    args.add("--output", output_dir.path)
    args.add("--target", target)
    args.add_joined("--sources", ctx.files.srcs, join_with = " ")

    ctx.actions.run(
        arguments = [args],
        inputs = ctx.files.srcs,
        outputs = [output_dir] + genfiles,
        env = {
            # We are not using ctx.bin_dir.path, which may be recommended for other cases
            "BAZEL_BINDIR": ".",
        },
        executable = ctx.executable.generate,
        progress_message = "[rtbot-generate] generating %s, target %s" % (ctx.label.name, ctx.attr.target),
    )

    return DefaultInfo(files = depset(genfiles))

_rtbot_generate = rule(
    implementation = _rtbot_jsonschema_impl,
    attrs = {
        "srcs": attr.label_list(
            allow_files = [".md"],
            default = [
              "//libs/std:md",
              "//libs/core:md"
            ],
        ),
        "generate": attr.label(
            executable = True,
            cfg = "exec",
            allow_files = True,
            default = "//tools/generator:generate",
        ),
        "target": attr.string(
            default = "jsonschema",
        ),
    },
)

def rtbot_generate(**kwargs):
    _rtbot_generate(**kwargs)
