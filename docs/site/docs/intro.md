---
sidebar_position: 1
---

# Introduction

`RtBot` A low latency, high efficiency, incremental analytical engine.

## What is?

`RtBot` is a framework, written in c++, to process streams of numerical data
in real time and to derive analytics from it with the lowest latency possible.
You can use it to detect anomalies, local peaks or to simply continuously transform
your data in real time, efficiently.

`RtBot` have been designed as an extensible dsp framework, where the building blocks
are called _operators_ and data flows through them according to the defined _connections_.
Programs are written declaratively in plain `json` (or `yaml`), which are essentially
definitions of a graph of operators with special input and output nodes.

# Use cases

If you need a digital signal processing (dsp) multiplatform framework which can trigger signals on
certain events happening on the input numerical stream then `RtBot` is for you.

Some use cases would be:

- Digital signal processing
- Low latency event detection
- Algorithmic trading

## How to effectively use it

`RtBot` has been designed to work effectively accross different phases of software
development, from the exploration over historical data to the production deployment
on a web app or into a micro-controller. As such, it can drastically reduce the cost
of producing software essentially due to the lack of translation phase.
This is perhaps one of the main reasons to adopt it.

Develop an `RtBot` program means finding the right operators, their configuration and how they
are connected, such that they produce the result you want as the data comes in. A common
workflow would be to use the `python` library to develop an `RtBot` program using historical
data, and then, once the program is ready, use the `JavaScript` version to deploy it into a web
app or the `c++` code directly to deploy it into any platform, depending on the needs.

# Wrappers

## JavaScript

`RtBot` code have been compiled into wasm and can be run both in node and the browser.
The published `npm` package has `TypeScript` support.

To install:

```shell
# npm
npm install --save-dev @rtbot-dev/rtbot
# yarn
yarn add @rtbot-dev/rtbot
# pnpm
pnpm add @rtbot-dev/rtbot
```

A simple example about how to use it in a node program can be found at [rtbot-dev/rtbot-example-ts](https://github.com/rtbot-dev/rtbot-example-ts).

A more interesting example can be found at [rtbot-dev/rtbot-example-websocket-ts](https://github.com/rtbot-dev/rtbot-example-websocket-ts).

## Python

The `python` wrapper have been designed with a data science public in mind. To install it:

```shell
pip install @rtbot-dev/rtbot
```

Note: we currently support only the following python versions:

- `v3.10.x`
- `v3.11.x`
- `v3.12.x`
