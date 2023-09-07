---
sidebar_position: 1
---

# Modules

RtBot framework is divided into a set of modules: _core_, _api_ and _std_ ones. This
list is likely to grow in the future and users can also extend it with their own
private business model specific ones.

The _core_ module contains the general definitions and it does not depend on any
other. You can write a program using only the core module, but you will have to
write it in c++ and explicitly construct the operators and their connections.

The _api_ module allows users to easily interact with the core module. For instance
it can read RtBot programs saved in json format and construct the correspondent
internal pipeline of operators at runtime. In this module is also where the
external bindings are defined, giving a simplified api to run programs. If you are
developing a web or desktop app, or even a mobile app, you will likely use this
module. On the other hand, if you are building an embedded application for a
microcontroller, including this module in your build will cause a large binary
output that probably won’t fit on the device. In this case you will have to rely
only on the core and std modules to make sure you produce binaries small enough.

The _std_ module represents a standard library of commonly used operators that can
be used to build programs. Operators defined here are general purpose ones, like
the inputs, resampler or finite response operators, which are ubiquitous used in
any kinds of input signals. In practice this module and the core one will be the
minimal components you will need in order to create some non trivial program.
