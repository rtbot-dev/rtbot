def _rtbot_jsonschema_impl(ctx):
    """
    The "implementation function" for our rule.

    It starts with underscore, which means this function is private to this file.
    Args:
        ctx: Bazel's starlark execution context
    """
    output_dir = ctx.actions.declare_directory("jsonschema")
    genfile = ctx.actions.declare_file("jsonschema/jsonschema.json")

    args = ctx.actions.args()
    args.add("--output", output_dir.path)
    args.add_joined("--files", ctx.files.srcs, join_with = " ")

    ctx.actions.run(
        arguments = [args],
        inputs = ctx.files.srcs,
        outputs = [output_dir] + [genfile],
        env = {
            # We are not using ctx.bin_dir.path, which may be recommended for other cases
            "BAZEL_BINDIR": ".",
        },
        executable = ctx.executable.generate,
        progress_message = "[rtbot-jsonschema] generate, args %s" % (args),
    )

    return DefaultInfo(files = depset([output_dir] + [genfile]))

_rtbot_jsonschema = rule(
    implementation = _rtbot_jsonschema_impl,
    attrs = {
        "srcs": attr.label_list(
            allow_files = [".cpp", ".h"],
            default = ["//libs/core/api:src/FactoryOp.cpp"]
        ),
        "generate": attr.label(
            executable = True,
            cfg = "exec",
            allow_files = True,
            default = "//tools/jsonschema:generate",
        ),
    },
)

def rtbot_jsonschema(
        **kwargs):
    _rtbot_jsonschema(**kwargs)
