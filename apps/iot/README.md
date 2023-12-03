# RtBot for Internet Of Things

(work in progress)

## Raspberry Pi Pico

It is important to notice that the existing `rules_pico` for bazel requires
certain binaries to be installed already, this is, the defined bazel toolchain
is not hermetic. In archlinux we need to install the following packages first:

```bash
sudo pacman -S arm-none-eabi-newlib arm-none-eabi-gcc
```

### Building

Use the following command to build an uf2 binary:

```bash
bazel build --config=pico //pico:hello.uf2
```
