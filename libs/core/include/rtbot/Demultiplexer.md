---
behavior:
  buffered: true
  throughput: variable
view:
  shape: demultiplexer
  latex:
    template: |
      \arrow
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator.
      examples: ["demul1"]
    numOutputPorts:
      type: integer
      description: The number of output ports through which data will be routed.
      default: 1
      minimum: 1
      examples: [2]
  required: ["id"]
---

# Demultiplexer

Inputs: `i1`  
Controls: `c1`...`cN` where N is defined by `numOutputPorts`  
Outputs: `o1`...`oN` where N is defined by `numOutputPorts`

A `Demultiplexer` is an operator that routes its incoming data through its possible output ports. 
The choice of the output port is controlled by the messages received through its control ports.

The `Demultiplexer` operator holds a message buffer on `i1` and `c1`...`cN` respectivily. A message 
received on `i1` with time $t_n$ will be routed through `ok` if a message with time $t_n$ and value 1 is 
recieved on `ck` and a message with time $t_n$ and value 0 is received on all controls different that `ck`. 
A message received on `i1` with time $t_n$ will be dropped if a message with time $t_n$ 
and value 0 is recieved on all of the control ports `c1`...`cN` respectively. It is important to notice 
that a `Demultiplexer` operator expects instructions on the control ports for all messages received on `i1`.

$$1 \leq k \leq N$$