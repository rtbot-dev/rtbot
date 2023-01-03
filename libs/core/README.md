# RtBot Core

This library contains the core/low level code of `rtbot` framework.

## Build and run

```bash
bazel build //libs/core/lib:rtbot
```

### Wrappers

#### Python

```bash
bazel build //libs/core/wrappers/python:rtbot.so
```

## Testing

```bash
bazel test //libs/core/test:rtbot
```
