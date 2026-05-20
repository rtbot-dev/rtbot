# rtbot JIT — parity with the framework

## Why parity matters

The JIT is a re-implementation of rtbot's canonical operator-graph framework.
Customers can flip between modes via `RTBOT_DISABLE_JIT=1`, and `Program`
constructs both at startup (the framework operator graph is always built; the
JIT is built on top when supported). For this to be safe the two modes must
produce **bit-identical emissions** for any program JSON we accept.

"Same emission" means same per-tick `(timestamp, port_id, values...)` over a
fixed input sequence. The bar is byte equality on values (`memcpy`-to-`uint64`
comparison — distinguishes NaN encodings and signed zero) and identical
sequencing of records.

This is the JIT's correctness contract. Every JIT bug we have found in
practice has fit one of two shapes:

  1. **Per-tick value drift** — different number for the same input. Usually
     accumulator format or FP arithmetic order. Easy to spot when you compare
     against the framework.
  2. **Per-tick emission drops / duplicates / mis-orders** — number of records
     differs, or records appear at the wrong tick or wrong port. Harder to
     spot because the visible values may still look reasonable. The dangerous
     case: the JIT silently produces zero output and downstream code thinks
     the pipeline is healthy.

Both shapes are caught by the same machinery: drive the same JSON through both
modes with deterministic input, byte-compare the emission stream.

## How we enforce it

Three layers in `libs/api/test/parity_helper.h` and the parity test files:

  1. **`run_jit_vs_interpreter_parity`** — the primitive. Builds two `Program`
     instances from the same JSON (one JIT, one with the runtime force-
     interpreter flag), drives both with the same input via a user callback,
     captures emissions, and `REQUIRE`s byte equality.

  2. **`libs/compiled/jit/test/test_framework_parity.cpp`** — pins the major
     shapes that mirror existing framework tests (basic graphs, MA chains,
     Pipeline, TriggerSet, Bollinger, RSI, resampler-thinned-input).

  3. **`libs/compiled/jit/test/test_framework_parity_audit.cpp`** — broader
     audit covering one representative SCENARIO per distinct operator-mix
     signature found across the framework test corpus. Includes asymmetric
     multi-port shapes, prototype expansion, nested pipelines, feedback
     loops.

When you add a new JIT emitter or change an existing one, add a parity
SCENARIO covering that shape. The cost is low (10 lines of test) and the
protection against silent regressions is high.

## Bugs found via parity that would have been silent otherwise

Documenting these so the bug shapes stay in collective memory:

### Resampler multi-emit (commit `bcc21d3`, 2026-05-19)

`SegmentEmitter`'s resampler segment wrote each callback emission into
single-slot allocas, so if the input had a gap larger than the resampler
interval (so the resampler fired N>1 times in one input tick), N-1 emissions
were silently overwritten and the segment returned `count=1` with only the
last value visible.

Caught by the Bollinger integration test (one of its assertions happened to
require consecutive output timestamps), which is how we noticed. Without
that assertion the JIT would have silently produced ~half the expected
emissions on any pipeline with thinned input and a resampler.

The fix: the resampler segment now writes each emission to indexed positions
in the K-slot output arrays and returns the per-call count, like every other
multi-emit op already did.

Class of bug: "callback that may fire 0..N times into per-segment scratch."
Any future emitter must use the K-slot arrays directly.

### Asymmetric multi-port Output drop (2026-05-20)

When a program's `Output` op has multiple input ports and one port emits on
ticks where another port does not (e.g. `port1 = LessThan(x, 5)` and
`port2 = GreaterThan(x, 5)` over a ramp), the JIT silently drops ALL
emissions. Framework correctly emits per-port independent of the other ports.

Caught by the audit (`test_framework_parity_audit.cpp`) — exactly the
scenario this layered safety net was built for.

Root cause: in `SegmentEmitter.cpp`, every stateful op's `emit_flag` was
ANDed into a single segment-wide `combined_flag` that gated the Output
write. For warmup flags (MA needs N ticks before emitting) this is correct —
parallel stateful branches warm together. For per-tick *conditional* flags
(the Filter ops `FiltGt`/`Lt`/`Eq`/`Neq` family) ANDing means "all ports'
predicates must hold simultaneously"; for asymmetric outputs (one port
active when count<5, another when count>5) that condition is never true
and the segment commits zero records.

Fix: split `emit_flag` into two buckets — warmup flags still AND into a
chain-wide guard (preserved behavior for windowed ops), conditional flags
are stamped per-`PortValue` and AND-propagated through stateless ops; at
Output we OR all input ports' flags as the commit gate and select(port_flag,
value, NaN) per slot so inactive ports get NaN-filled, matching the
framework's `drain_into` NaN-fill record shape.

Class of bug: "Output erroneously sync-gated like a Join."

Framework port-sync taxonomy (verified by reading source):

  Sync ops (emit only when all input ports have ticks at the same t):
    Join, ReduceJoin (and its subclasses: Addition/Subtraction/...,
    LogicalAnd/Or/..., FilterSync family, CompareSync family, BooleanSync
    family), Linear, VectorCompose, FusedExpression, Demultiplexer (over
    control ports), Multiplexer.

  Per-port independent (each port drains on its own ticks):
    Output, TriggerSet.

The JIT bug ANDed every per-tick conditional flag from upstream ports into
a single commit gate, which is correct for sync ops but wrong for Output.
The fix preserves the sync behavior for sync ops (warmup-flag AND'ing stays
chain-wide) and changes only the commit gate at Output to OR the per-port
flags.

**Suspected sibling bug, not yet exercised**: `emit_program`'s Pipeline
output path has the same AND-into-`should_emit_alloca` pattern. A Pipeline
whose inner op feeds asymmetric Filter branches to a multi-port **outer
Output** would hit this. (Inner ops feeding a Join or other sync op are
fine — that's the intended AND behavior.) We don't have a parity test for
the Pipeline→outer-Output asymmetric case yet — add one when extending
audits.

### Per-port warmup-flag plumbing (commits `294af88`, `e1c6abe`, `7c1456b`,
### `490a77c`)

Added a `warmup_flag` to `PortValue` plumbed end-to-end:
- Stateful ops stamp their `emit_flag` onto the PortValue they produce.
- Stateless ops AND-propagate the flag from their inputs.
- The Output op's commit gate ORs the per-port flags (so asymmetric ports
  still commit when ANY port is live) and NaN-masks inactive port slots.
- Stateful chains (filter → filter → ...) AND upstream `warmup_flag` into
  the downstream stateful op's flag so framework suppression survives
  through the chain.
- `drain_into`'s JIT branch groups per-port-discriminator records (Demux,
  Mux, TopK) by timestamp so framework's per-tick aggregation shape is
  matched.

Class of bug: "different kinds of `emit_flag` (warmup vs conditional)
conflated into a single global commit gate". The earlier fix (`bcc21d3`)
applied the OR shape to the resampler path only; subsequent fuzz audit
caught ~25 more residual cases that share the same root and were fixed
by porting the per-port plumbing into the segment-ops and emit_segment
code paths.

## Known remaining divergence classes (next-session work)

After the warmup-flag plumbing, the fuzz harness still finds three small
bug classes at low frequency (~10 across seeds 42/100/999 of 600 random
programs):

1. **Stateful ops advance state on upstream-garbage values.** A windowed
   op (MA/StdDev/MovingSum/etc.) unconditionally runs its state-advance
   IR every tick, using whatever the upstream computed even when the
   upstream's `warmup_flag` is false (would have suppressed in the
   framework). Result: the JIT op is 1+ ticks ahead of the framework op
   on warmup. Fix path: branch the state-advance IR on upstream's
   `warmup_flag`, or save/restore the state slots via `select`. Either
   is invasive (5-10 state slots per windowed op type) and should be
   done op-by-op.

2. **Output downstream of unrelated sync op.** Graph partitioning places
   Output in a sync op's downstream segment when Output's input ports
   don't actually depend on that sync op. Result: Output skips emissions
   when the unrelated sync hasn't fired. Fix path: graph-partition
   refactor to keep Output's segment independent when possible. Example:
   pipeline with dead-code Join feeding nothing observable but still
   placing Output in Join's downstream segment.

3. **LLVM IR codegen crashes ("Terminator in middle of basic block").**
   On certain shapes (sync 2-port op consuming filter-gated comparator
   upstream) the JIT emits malformed IR which LLVM rejects with
   `report_fatal_error` (uncatchable abort). Fix path: trace the IR
   structure for one repro; identify which emitter produces the
   malformed CFG; fix the branch placement.

Reproductions are in `/tmp/fuzz_failure_<seed>_p<idx>.json`; re-run via
`bazel test ... --test_arg='[fuzz]' --test_env=RTBOT_FUZZ_SEED=...`.

## Known ASan caveat

rtbot tests run with `ASAN_OPTIONS=detect_container_overflow=0` (set via
BUILD.bazel `test_env`). The 3 resampler-thinned-input parity scenarios in
`test_framework_parity.cpp` crash inside LLVM's `TypePromotionLegacy::
runOnFunction` with a container-overflow report when that suppression is off.

This is a known false positive: ASan-instrumented user code calls into
un-instrumented LLVM library code which legitimately reads from preallocated
`SetVector` capacity. The crash happens during teardown after the parity
checks have already passed. Correctness is unaffected.

Fix is upstream LLVM (build LLVM with ASan, which is impractical). Do not
chase from rtbot.
