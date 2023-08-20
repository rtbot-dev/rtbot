---
sidebar_position: 5
---

# Join operators

Join operators combine two streams of messages into a single one. It tries to resemble `sql` join operation,
where two tables are combined into a new one. They are perhaps RtBot special ingredients,
and will be used whenever we need some sort of synchronization to happen between two signals.

TODO: show table

These operators can have multiple inputs, each one bound internally to a port. They can have any number of _ports_,
or input channels, from where the streams of other operators can be received. Messages received through the ports
are queued efficiently inside the `Join` operator. The operator will emit whenever it receives
a message in any of the ports with time $t$ and there exists _at least one message_ across _all_ the queues with that
time. In such a scenario, all these messages with same timestamp are popped out from the internal queue, and emitted.
The queue also remove all the old messages that there is no chance that can be emitted
because they are too old, keeping the process memory efficient.
