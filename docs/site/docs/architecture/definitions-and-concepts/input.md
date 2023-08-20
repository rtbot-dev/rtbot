---
sidebar_position: 4
---

TODO: finish the merge between the resamplers and the input

# Input

An RtBot program is then this set of operators connected in a graph structure,
where the external signal entry points are marked by the input
operator. The input operator is just an operator that is designed to be a good
intake of the external signal. Programs must have at least an input operator,
which will perform sanity checks on the input signal before forwarding
the message to others. For example, an input operator will discard new messages
with timestamps prior or equal to the last message received, guaranteeing the time
order in the messages it forwards.

## Resampling

All operators that implement some math over their input assume that the messages
it will receive are not only time ordered but also _equidistant in time_, or _equally
spaced in time_. We will call this type of signal a regular one, while those that
don't fulfill this will be named irregular. This is a strong assumption and it
will likely be the case that input signals are irregular. This is why we have a
special set of operators called resamplers.

A resampler operator will take an irregular input signal and will produce a regular one.
Internally, it will use the irregular signal as its source of truth, to find a realistic interpolation
over the time grid points. The interpolation algorithm depends on the resampler
used. In practice this means that, while not strictly necessary, programs will
start with input operators followed by resampler ones.

TODO: display example table
