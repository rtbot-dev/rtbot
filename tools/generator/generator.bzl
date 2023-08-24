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
        genfile = ctx.actions.declare_file("%s/jsonschema.json" % ctx.label.name)

    if target == "cpp":
        genfile = ctx.actions.declare_file("%s/jsonschema.hpp" % ctx.label.name)

    if target == "python":
        genfile = ctx.actions.declare_file("%s/jsonschema.py" % ctx.label.name)

    if target == "typescript":
        genfile = ctx.actions.declare_file("%s/index.ts" % ctx.label.name)

    output_dir = ctx.actions.declare_directory(ctx.label.name)

    args = ctx.actions.args()
    args.add("--output", output_dir.path)
    args.add("--target", target)
    args.add_joined("--sources", ctx.files.srcs, join_with = " ")

    ctx.actions.run(
        arguments = [args],
        inputs = ctx.files.srcs,
        outputs = [output_dir] + [genfile],
        env = {
            # We are not using ctx.bin_dir.path, which may be recommended for other cases
            "BAZEL_BINDIR": ".",
        },
        executable = ctx.executable.generate,
        progress_message = "[rtbot-generate] generating %s, target %s" % (ctx.label.name, ctx.attr.target),
    )

    return DefaultInfo(files = depset([genfile]))

_rtbot_generate = rule(
    implementation = _rtbot_jsonschema_impl,
    attrs = {
        "srcs": attr.label_list(
            allow_files = [".md"],
            default = [
              "//libs/core/std:md",
              "//libs/core/lib:md"
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
