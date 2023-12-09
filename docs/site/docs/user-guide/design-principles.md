---
sidebar_position: 1
---

# Design principles

In order to create a meaningful new computation system that takes time into account RtBot has been designed
according to the following principles:

_Causality_. Only the past can determine the future. Internally it is assumed that operators can only receive
messages ordered in time. In real life, data providers may not guarantee this and it can be possible that two
messages with the same timestamp are received. For this case we have an special operator: `Input` whose only
responsibility is that the operators behind it receive messages with an increasing timestamp, discarding input
data if necessary.

_Deterministic_. Same input produces the same output. Here input and output refers to entire message streams and not
to individual messages. If we start our program and feed a list of messages we expect to get the same list of output
messages. Now in order to guarantee this at the program level we impose this at the operator level, which means that
the output of the operator shall not depend on when its inputs arrive. For many operators this is trivial, like for
example the `MovingAverage` one, but for certain operators, specially those that perform synchronization among different
streams of messages, this implies constraints on its outputs.

_Declarative_. The behavior of the program is determined by the operators and how they are connected. In other words,
the behavior of the program shall not be determined by in which order operators were connected but solely by the topology
of the graph of operators.
