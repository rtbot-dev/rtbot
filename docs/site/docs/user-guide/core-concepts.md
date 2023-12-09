---
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
