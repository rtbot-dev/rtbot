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
      examples: ["demult1"]
    numPorts:
      type: integer
      description: The number of output ports through which data will be routed.
      default: 1
      minimum: 1
      examples: [2]
  required: ["id"]
---

# Demultiplexer

Inputs: `i1`  
Controls: `c1`...`cN` where N is defined by `numPorts`  
Outputs: `o1`...`oN` where N is defined by `numPorts`

A `Demultiplexer` is an operator that routes its incoming data through its possible output ports based on control signals. The operator can route messages to multiple outputs simultaneously when multiple control signals are active.

The `Demultiplexer` operator holds a message buffer on `i1` and `c1`...`cN` respectively. A message received on `i1` with time $t_n$ will be routed through any output port `ok` if a message with time $t_n$ and value `true` is received on control `ck`. The message will be dropped if all control messages at time $t_n$ have value `false`. A `Demultiplexer` operator expects control messages for all ports at each timestamp where data messages are present.

Key behaviors:

- Messages can be routed to multiple outputs simultaneously
- Messages on each output port must have strictly increasing timestamps
- Control messages are required for all ports at each data message timestamp
- Messages are dropped when no control signals are active
- Control values of `true` indicate active routing, `false` indicates inactive

Example behavior:

```
Time  Controls  Data   Outputs
100   1,0,0    42.0   o1: 42.0
200   0,1,1    24.0   o2: 24.0, o3: 24.0
300   1,1,1    36.0   o1: 36.0, o2: 36.0, o3: 36.0
400   0,0,0    48.0   (dropped)
```

## Port Configuration

All output ports are of the same type as the input port. Control ports are always boolean type.

## Message Synchronization

The operator maintains timestamp synchronization across all ports:

- Control messages are required for each timestamp where data is present
- Control messages must be present on all control ports for a given timestamp
- Messages on each output maintain chronological order
- Late messages (lower timestamps than previously emitted) are dropped

## State Management

The operator maintains:

- Data message buffer
- Control message buffers
- Timestamp tracking for synchronization

$$1 \leq k \leq N$$ where N is the number of output ports
