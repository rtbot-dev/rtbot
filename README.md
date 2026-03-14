# RtBot

[![License](https://img.shields.io/badge/license-BUSL--1.1-blue)](LICENSE)
[![GitHub release](https://img.shields.io/github/v/release/rtbot-dev/rtbot)](https://github.com/rtbot-dev/rtbot/releases)
[![npm](https://img.shields.io/npm/v/%40rtbot-dev/rtbot)](https://www.npmjs.com/package/@rtbot-dev/rtbot)
[![CI](https://img.shields.io/github/actions/workflow/status/rtbot-dev/rtbot/release.yaml)](https://github.com/rtbot-dev/rtbot/actions/workflows/release.yaml)
[![Discord](https://img.shields.io/discord/1097198588490162246)](https://discord.gg/XSv6mZq7YQ)

The real-time computation engine behind [RtBot SQL](https://github.com/rtbot-dev/rtbot-sql). A C++ framework that processes streams of numerical data one message at a time using directed graphs of operators — with constant-time updates, deterministic output, and zero infrastructure dependencies.

- Site: [www.rtbot.dev](https://www.rtbot.dev)
- Repo: [github.com/rtbot-dev/rtbot](https://github.com/rtbot-dev/rtbot)

## Why RtBot

RtBot is the engine that powers [RtBot SQL](https://github.com/rtbot-dev/rtbot-sql). While most users should start with the SQL interface, the core engine is the right choice when you need:

- **Fine-grained operator control** — build custom signal processing chains that go beyond what SQL can express
- **Custom operators** — extend the engine with Lua scripting or C++
- **Embedded deployment** — run on microcontrollers (ARM, Raspberry Pi Pico) or directly in C++ applications
- **Program prototypes** — define reusable parameterized subgraphs for complex, repeated patterns

### Design principles

**Causal.** Only the past determines the future. Operators receive messages ordered in time and can only look backward — no future data, no lookahead.

**Deterministic.** Same input always produces the same output. The system uses event time exclusively — no wall clocks, no watermarks, no timing-dependent behavior. Replay yesterday's data and get identical results.

**Declarative.** Program behavior is determined by the topology of the operator graph, not by the order operators were created or connected. Programs are plain JSON.

**Incremental.** Every operator maintains internal state and updates it in constant time per message. A `MovingAverage` with window 10,000 is just as fast per message as one with window 10.

## How it works

An RtBot program is a directed graph of operators. Data enters through `Input`, flows through operators that transform it, and exits through `Output`. Each operator receives a timestamped message, updates its internal state in constant time, and emits zero or more messages downstream. No batching, no micro-batching, no scheduling — pure incremental processing.

The operator graph is a JSON structure that runs identically in every environment:

```
┌────────────────────────┐
│  Operator Graph (JSON) │
└───────────┬────────────┘
            │
    ┌───────┼───────────────┬──────────────────┐
    ▼       ▼               ▼                  ▼
┌────────┐ ┌────────────┐ ┌────────────┐ ┌──────────┐
│Browser │ │   Python   │ │   Redis    │ │   C++    │
│ (WASM) │ │  (Native)  │ │  (Module)  │ │ (Native) │
└────────┘ └────────────┘ └────────────┘ └──────────┘
 Demos      Notebooks      Production     Embedded
```

Prototype in a notebook, validate against historical data, deploy to Redis — no code rewrite, no behavioral differences.

## RtBot SQL

For most use cases, [RtBot SQL](https://github.com/rtbot-dev/rtbot-sql) provides a higher-level interface. Write SQL instead of JSON operator graphs:

```sql
CREATE MATERIALIZED VIEW stats AS
  SELECT temperature,
         MOVING_AVERAGE(temperature, 50) AS avg_temp,
         MOVING_STD(temperature, 50)     AS std_temp
  FROM sensors
```

RtBot SQL compiles queries into operator graphs that execute on this engine. Same performance, same determinism, less boilerplate.

## Development

```bash
bazel build //...
bazel test //...
```

Build artifacts go to `dist/` (configured in `.bazelrc`).

## Documentation

Full documentation, guides, and API reference at [www.rtbot.dev](https://www.rtbot.dev).

## License

This project is licensed under the Business Source License 1.1 (`BUSL-1.1`).

- Licensor: `rtbot-dev`
- Change Date: `2029-03-10`
- Change License: `Apache-2.0`

See `LICENSE` for full terms.
