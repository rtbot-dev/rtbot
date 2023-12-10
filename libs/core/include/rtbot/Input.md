---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      \circ
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    numPorts:
      type: integer
      examples:
        - 20
      description: The number of possible input ports. Useful if more than one input is taken.
      default: 1
      minimum: 1
  required: ["id"]
---

# Input

Inputs: `i1`...`iN` defined by `numPorts`  
Outputs: `o1`...`oN` defined by `numPorts`

The `Input` operator is used to ensure that only messages with increasing timestamp
are sent to the rest of the program. 

In certain scenarios data received from the outside world may arrive not time-ordered, 
or messages with same timestamp might be received consecutively. In such scenarios the `Input` operator 
will discard invalid messages to ensure the proper functioning of the operator pipeline 
behind it. The `Input` operator does not hold a message buffer on any of the ports defined, 
so it forwards the received message on the port `ik` through the port `ok` right after. 

$$1 \leq k \leq N$$

The received messages are not buffered therefore it has a little footprint on the calculation process.