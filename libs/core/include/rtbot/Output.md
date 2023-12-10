---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \bullet

jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    numPorts:
      type: integer
      description: The number of ports.
      default: 1
      minimum: 1
  required: ["id"]
---

# Output

Inputs: `i1`...`iN` defined by `numPorts`  
Outputs: `o1`...`oN` defined by `numPorts`

The `Output` operator is used to push data out of the program.

It is considered good practice to collect all resulting streams in an `Output` operator. The `Output` operator 
does not hold a message buffer on any of the ports defined, so it forwards the received messages on the port 
`ik` through the port `ok` right after. 

$$1 \leq k \leq N$$

The received messages are not buffered therefore it has a little footprint on the calculation process.