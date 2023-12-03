# Testing

Api functionality can be tested by running:

```bash
bazel test //libs/core/api/test
# notice that this is just a shorter version of
# bazel test //libs/core/api/test:test
```

If you want to see the output of the test in the `stdout` run:

```bash
bazel test --test_output=all //libs/core/api/test --test_arg=-s --test_arg="-r compact"
```

You can play with the `catch2` cli arguments by adding more `--test_arg` values
in the above command.
