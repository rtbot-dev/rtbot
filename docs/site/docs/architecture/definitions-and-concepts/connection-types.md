---
sidebar_position: 3
---

# Connection types

There are two types of connections in RtBot: _data lines_ and _control lines_.
Data lines are connections through which the incoming data or derived passes through. The messages
passing through the data lines are usually a tuple of an integer, representing the time, and a float
number, representing a value.

On the other hand, control lines are used to control the data flow. They are used mainly to take
decisions, like switching the flow from one data line to another, for instance.
Through these lines we send messages as well, with the difference that these messages are usually a tuple
of integers: one for the time and another integer which can be used in the decision taking process. This
second integer can represent a given state in our program, or a boolean, for instance.
