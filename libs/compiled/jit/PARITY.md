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

The fuzz harness used to abort at the first class-3 LLVM crash, hiding
how many other divergences existed. With class-3 fixed it now runs to
completion: seed=42, 200 generated → 64 JIT-supported → 55 pass / 9 fail
(all `count_mismatch`). The 9 failing pipelines (p42, p71, p116, p136,
p138, p154, p179, p193) split between classes 1 and 2 below.

1. **Stateful ops advance state on upstream-garbage values.** A windowed
   op (MA/StdDev/MovingSum/etc.) unconditionally runs its state-advance
   IR every tick, using whatever the upstream computed even when the
   upstream's `warmup_flag` is false (would have suppressed in the
   framework). Result: the JIT op is 1+ ticks ahead of the framework op
   on warmup. Fix path: branch the state-advance IR on upstream's
   `warmup_flag`, or save/restore the state slots via `select`. Either
   is invasive (5-10 state slots per windowed op type) and should be
   done op-by-op.

2. **Output downstream of unrelated sync op.** Once the class-3 partition
   fix moved `Output` to the tail of the LAST segment, `emit_program`
   started emitting that segment inside the LAST sync op's `then_bb` —
   so when the sync never fires (e.g. a filter-gated comparator chain
   feeding the sync's i2 produces no value), `Output` is skipped even
   though its data ports may have nothing to do with that sync.
   Symptom: pipeline 42 (`Linear(Floor(input), EqualTo<-NotEqualTo)` with
   `Output` fed from `Multiplication(input, input)`) now compiles but
   reports `jit=0 interp=200` in the fuzz harness.
   A naïve fix (extract `Output` from segments and emit it unconditionally
   after the sync ops loop) breaks SSA dominance for `Output`'s ports
   that ARE fed by a sync op — their value_map entries are SSA from the
   sync's `then_bb`, and a post-loop block isn't dominated by `then_bb`.
   The proper fix needs PHI nodes at each `merge_bb` that merge the
   then-branch SSA value with a `NaN`/undef sentinel for the no-sync
   branch, plus per-PortValue `warmup_flag = sync_flag` so the existing
   per-slot NaN mask kicks in when the sync didn't fire. Tracked as
   next-session work.

3. **LLVM IR codegen crashes ("Terminator in middle of basic block") —
   FIXED (2026-05-20).** Topo sort could place the `Output` op in a
   segment *before* a downstream non-Output op (legal because Output is a
   sink with no outgoing data edges). `emit_segment_ops` then emitted
   Output, terminated its basic block with `br ret_true`, and continued
   appending the next op's IR — including the next sync op's
   `br jts_loop_hdr` — into the already-terminated block. LLVM rejected
   the malformed CFG via `report_fatal_error` (uncatchable abort).
   Repro: `/tmp/fuzz_failure_42_p42.json` and
   `test_fuzz_repro_diag.cpp` SCENARIO `fuzz repro: p42 (Linear(Floor,
   EqualTo<-NotEqualTo) - LLVM IR crash)` under `[fuzz_repro_p42_crash]`.
   Fix: `partition_segments` (`SegmentPartitioner.cpp`) defers every
   `Output` op across the topo walk and appends them at the tail of the
   final segment so the Output's `br ret_true` is always the last
   instruction emitted in the function body. As a side-effect this also
   ensures every sync op gets to advance its queue state before the
   function exits. Diagnostic: setting
   `RTBOT_JIT_DUMP_ON_VERIFY_FAIL=1` dumps the offending function to
   stderr from `emit_program` before LLVM's `report_fatal_error` — keep
   this around for the next class-3 we find.

4. **Per-port timestamp drift via retroactive stateful ops — FIXED
   (2026-05-20).** When a multi-port `Output` consumes from a stateful op
   whose `out_t` may differ from the segment's input tick
   (`PeakDetector` writes the window-center timestamp; `TimeShift` writes
   `t + shift`), the pre-fix linear-segment Output IR
   (`SegmentEmitter.cpp:1086–1120`) stamped the single emitted record
   with `in_conns[0]->t` — the segment's input tick — and wrote every
   live port's value into that record. The retroactive value reached the
   record but the retroactive timestamp was lost; the framework emits
   that value at its own logical `t` in a separate record.
   Repro: `/tmp/fuzz_failure_42_p15.json` (`Abs -> {MovingSum,
   PeakDetector} -> 2-port Output`). Pre-fix JIT: 40 records, all
   stamped at the input tick. Framework: 52 records, with PD's value at
   the window-center tick (e.g. 98.2335 at t=2, not t=5).
   Fix: a compile-time gate
   (`use_per_port_multi_emit = has_retroactive_t_op(graph) &&
   output_op_input_port_count(graph) > 1`, both in `CompiledGraph.h`)
   switches the linear-segment Output to per-port multi-emit. Each live
   input port writes one K-slot record at its own `t`, with the value at
   K-slot 0 (vector lanes spread starting at K-slot 0), `port_id` =
   port's flat-slot base, and remaining K-slots NaN-filled. The segment
   returns the live-port count via `al_per_port_count`. `compute_max_
   emits_per_call` in `StateLayout.cpp` bumps K to the Output's input
   port count when the retroactive condition holds. `Program::compile_
   jit_` sets `jit_per_record_port_dispatch_ = true` when retroactive +
   multi-port, so `drain_into`'s per-port aggregation reassembles
   same-`t` records into the framework's combined-row shape (NaN-fills
   ports that did not emit at that `t`). Regression test:
   `test_fuzz_repro_diag.cpp` SCENARIO
   `parity: per-port retroactive-t Output (PeakDetector multi-port)`
   under the default `[program][jit][framework_parity]` tags.

Reproductions are in `/tmp/fuzz_failure_<seed>_p<idx>.json`; re-run via
`bazel test ... --test_arg='[fuzz]' --test_env=RTBOT_FUZZ_SEED=...`.
Self-contained side-by-side diagnostics for three concrete repros (p15,
p33, p39) live in `test/test_fuzz_repro_diag.cpp` under the
`[fuzz_repro]` tag (not part of the default test run; invoke with
`--test_arg='[fuzz_repro]'`).

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
