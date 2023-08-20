---
sidebar_position: 2
---

# Operators

The main concept in the RtBot architecture is that of an _operator_. An operator
it’s simply a computational unit that takes a message as input, transforms it and produces
messages as output. Operators assume they will receive messages with
increasing timestamps, this is, ordered in time.

Operators can be connected with other operators, in a directed way, such that one
operator can have many children operators. Operator have _ports_ on it, which control
the incoming and outgoing traffic from and to the operator. More specifically, operators
are connected by creating a connection between some _outbound_ port on the parent
operator and some _inbound_ port on the child operator. Data send to outbound ports will be then
sent to all the child operators connected to that port, and it will be received in the inbound
port specified by the connection.
There may exist several connection between two operator, as long as
the combination of outbound and inbound port is unique.

Operators may change the throughput of the
stream they receive, producing any number of messages, or none, per each message
it receives. In this sense they are also flow controllers, deciding when and what
to forward to its children operators.
