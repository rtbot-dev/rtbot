# Testing

Core functionality can be tested by running:

```bash
bazel test //test:rtbot
```

If you want to see the output of the test in the `stdout` run:

```bash
bazel test --test_output=all //test:rtbot --test_arg=-s --test_arg="-r compact"
```

You can play with the `catch2` cli arguments by adding more `--test_arg` values
in the above command.
