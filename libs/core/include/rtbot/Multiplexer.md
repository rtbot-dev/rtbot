---
behavior:
  buffered: true
  throughput: variable
view:
  shape: multiplexer
  latex:
    template: |
      \arrow
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    numPorts:
      type: integer
      description: The number of input ports through which data can be routed.
      default: 2
      minimum: 2
      examples: [2]
  required: ["id"]
---

# Multiplexer

Inputs: `i1`...`iN` where N is defined by `numPorts`  
Controls: `c1`...`cN` where N is defined by `numPorts`  
Output: `o1`

A `Multiplexer` is an operator that selectively routes one of its input streams to its single output port. The selection is controlled by messages received through its control ports.

The `Multiplexer` operator holds a message buffer on `i1`...`iN` and `c1`...`cN` respectively. A message with time $t_n$ received on input port `ik` will be routed through `o1` if:

1. A message with time $t_n$ and value 1 is received on control port `ck`
2. Messages with time $t_n$ and value 0 are received on all other control ports
3. Messages with any other values on control ports will cause no output

In other words, exactly one control port must have value 1 (selecting its corresponding input) while all others must have value 0 for routing to occur. The Multiplexer requires control messages for all ports to be present at the same timestamp before it will route any data.

This behavior makes the Multiplexer complementary to the Demultiplexer operator - while a Demultiplexer routes one input to multiple outputs, a Multiplexer routes multiple inputs to one output.

$$1 \leq k \leq N$$
