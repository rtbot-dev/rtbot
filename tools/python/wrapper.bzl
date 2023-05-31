def _rtbot_python_wrapper_impl(ctx):
    """
    The "implementation function" for our rule.

    It starts with underscore, which means this function is private to this file.
    Args:
        ctx: Bazel's starlark execution context
    """
    output_dir = ctx.actions.declare_directory("wrapper")
    genfile = ctx.actions.declare_file("wrapper/lib.cpp")

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
        progress_message = "[rtbot-python] generate, args %s" % (args),
    )

    return DefaultInfo(files = depset([output_dir] + [genfile]))

_rtbot_python_wrapper = rule(
    implementation = _rtbot_python_wrapper_impl,
    attrs = {
        "srcs": attr.label_list(
            allow_files = [".cpp", ".h"],
        ),
        "generate": attr.label(
            executable = True,
            cfg = "exec",
            allow_files = True,
            default = "//tools/python:generate",
        ),
    },
)

def rtbot_python_wrapper(
        srcs,
        **kwargs):
    _rtbot_python_wrapper(srcs = srcs, **kwargs)
