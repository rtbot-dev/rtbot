---
title: Concepts
sidebar_position: 2
---

# Concepts

An RtBot program expects a _signal_ as input, which is a time-ordered stream of
messages to be fed to it, one at the time, in what we call a _program iteration_.
This iteration may or may not produce an output, which will be defined later.

A _message_ is a tuple of timestamp and value. It is important to remark, even if
obvious, that the timestamp in the message is the one used for any internal
analysis, and not the time that the message arrives. RtBot has no clock inside or
anything similar, so it has no way of counting real life time.

RtBot assumes the time inside a message to be an integer. The main motivation for
this is that comparing two timestamps is more efficient and meaningful in this
case than if we allow the timestamps to be of float type. This allows, for
instance, to synchronize signals according to its timestamp in a much simplified
and efficient way. This shall not, however, be a hurdle to adopt RtBot to process
signals with fractionary timestamps, as a rescaling in the time dimension of the
signal prior to sending it to the RtBot program will solve any type mismatch. This
time scale, i.e. seconds or milliseconds, depends on the nature of the signal
being processed and it is up to the programmer to decide which makes sense for the
given type of signal.


## Input queue limits

Operators can retain input while waiting for another data or control port. For
example, a `Variable` retains sensor history until queries arrive, and a join
waits for matching timestamps. A filter that stops emitting can therefore leave
other inputs queued indefinitely.

Set these environment variables **before creating programs**:

| Variable | Default | Meaning |
| --- | --- | --- |
| `RTBOT_MAX_QUEUE_MESSAGES` | `17280` | Maximum messages per data or control port; `0` explicitly allows unlimited buffering. |
| `RTBOT_QUEUE_RESTORE_OVERFLOW` | `error` | Separate recovery policy: `drop_oldest` explicitly discards oversized saved input history while restoring. |
| `RTBOT_QUEUE_OVERFLOW` | `error` | `error` rejects the incoming message with the operator and port identified; `drop_oldest` removes the oldest queued message to admit the new one. |

For the legacy bounded-history behavior, use:

```sh
RTBOT_MAX_QUEUE_MESSAGES=17280 RTBOT_QUEUE_OVERFLOW=drop_oldest your-rtbot-process
```

`drop_oldest` can change results for late queries or matching inputs: discarded
history is no longer available. At one message every five seconds, 17,280 messages
represent approximately one day of history. Choose this policy only when that
loss is acceptable. `error` bounds memory without silently discarding history;
the hosting application must handle the exception and mark processing unhealthy.
Delivery to multiple operators or a batch is not transactional: after an error,
do not retry on the partially advanced graph. Recreate and restore a consistent
checkpoint before replaying with the selected policy.

C++ callers can configure individual operators before ingress using
`op.set_queue_limits({17280, rtbot::QueueOverflowPolicy::DropOldest})`, and inspect
`op.queue_statistics(kind, port)` for peak, dropped, and rejected message counts.
These statistics are local runtime counters, not exported service metrics.
The bound applies to direct ingress and optimized connected delivery, including
batches. Output collectors remain unbounded so they return complete results;
the caller must drain them regularly. Operator-specific buffers, output/debug
buffers, and message payload size are not covered by this message-count limit.

Queue policy and counters are not serialized. Restore with the intended policy;
replay with a different policy may produce different results. By default restore rejects
snapshots containing an input queue above the configured limit, even under
`drop_oldest`, rather than silently truncating persisted state. Existing oversized
snapshots require an explicit recovery/migration plan. After retaining a backup,
`RTBOT_QUEUE_RESTORE_OVERFLOW=drop_oldest` permits bounded restore, keeps the
newest messages, and counts discarded history. This is a lossy recovery option. This setting does not
control Redis stream retention (`RTBOT_MAX_STREAM_SIZE`) or the module's snapshot
size limit, and requires an engine build containing queue-limit support.
