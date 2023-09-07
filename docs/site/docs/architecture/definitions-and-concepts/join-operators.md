---
sidebar_position: 5
---

# Join operators

Join operators combine two streams of messages into a single one. It tries to resemble sql join operation, where two
tables are combined into a new one. These types of operators are one of RtBot's special ingredients.

## Motivation

Before describing the existing Join operators in RtBot let us first clarify the problem we are addressing here. Let’s
consider that we want to build an operator that combines two streams into one. In order to have operators that can
handle more than one input we introduce the concept of ports, which indicate the possible entry and exit points of an
operator. This way an operator can handle multiple inputs by receiving the data through its input ports. At the same
time, operators can send multiple outputs through their output ports. Operator connections are always from output ports
to input ports. Ports are identified by their id, and we conventionally denote input ports as `i1`, `i2`, … etc. and
output ports as `o1`, `o2`, … etc.. For each input port there is an internal queue where the incoming messages are
queued until they are used in the synchronization algorithm. This way we ensure there is no data loss in this process.

Let us focus now on the simplest case where we want to join only two streams, entering through the ports `i1` and `i2`.
A `Join` operator will take the messages received in previous iterations at `i1` and `i2` and will combine those to emit
values through `o1` and `o2` in the same iteration. What looks like a trivial task indeed is not, mainly because
messages may arrive in different iterations and because we can have streams with mismatching timestamps patterns.

To set some ground, let's check first the simplest case where we have two incoming streams whose timestamp pattern
matches. Consider still that messages may arrive at the input ports in different iterations. In that case, whenever we
receive a message through `i1` we check if we have another message already in `i2` with the same timestamp. If there is,
we emit the tuple with the common time using the output ports and remove both from their respective input queues. If not
we do nothing and we wait until the next iteration. For messages arriving to `i2` we do something similar. Recall that
this algorithm respects our deterministic principle: no matter in which order the messages arrive, if an auditor
watching the output writes it into a table he will get the same result if we send the input data in different
iterations. He may have to wait more or less iterations to complete the table but its content will be the same: a table
with columns $t$, $o_1$ and $o_2$ where each row have a timestamp and the correspondent values of the messages that
entered through `i1` and `i2`. This algorithm’s result is invariant against the data receiving pattern: we get the same
result if we send all data with destiny `i1` in a single iteration or if we send one message per iteration. The result
is the same as if in the world of sql databases we create and populate two tables for the inputs `i1` and `i2` and we
perform an inner join considering the time as the common column.

TODO: show figure.

Now let’s consider a more interesting and common scenario: we would like to join two streams whose timestamp pattern
doesn’t match. This means: for each message received in `i1` sometimes will arrive a message in `i2` with the same
timestamp. In other words only a subset of messages that will ever be received through `i1` or `i2` have a common
timestamp. There are some options here about how to produce an output in a deterministic way, which will be described below.

The first option is to emit only messages that have common timestamps in both incoming streams of messages. This is the
default behavior of the RtBot Join operator. Whenever we get a message through `i1` we look for messages in `i2` with
the same timestamp. If we find a pair, we emit the tuple and remove both messages from their respective queues as
explained above. But in this case, given that we are guaranteed to receive messages with increasing timestamp through
the ports thanks to our causality axiom, we remove all the messages from `i2` that have a timestamp less than the one in
the message received, as we are sure we won’t receive any message through `i1` that we can pair with those. This way we
keep our queues working efficiently, storing only what is needed. The same happens if the message arrives through `i2`,
we just need to swap the indices. This algorithm is also invariant with respect to the order of data arrival. An auditor
writing the output in a table will see that the result again is the same as if we were to use sql to perform an inner
join between the input tables with the time as a common column.

TODO: commen the other options and how they are absorbed with the variable definition plus the previous one.
