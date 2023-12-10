---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      \cap
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    numPorts:
      type: integer
      description: The number of input ports.
      default: 2
      minimum: 2
  required: ["id"]
---

# Join

Inputs: `i1`...`iN` where N defined by `numPorts`  
Outputs: `o1`...`oN` where N defined by `numPorts`

The `Join` operator is used to synchronize two or more incoming message streams into
a single, consistent output.

The `Join` operator holds a message buffer on `i1`, `i2`, ... , `iN` respectively, 
it uses the message time field to synchronize the streams and pick 1 message per input port. 
If at least one of the message buffer is empty or a message with the expected time can not 
be found on one of the buffers then the synchronization will not occur. When synchronization 
occurs messages with older timestamps than the synchronization time are discarded since it is 
understood that they can not be synchronized in the future. The `Join` operator emits the 
synchronized messages through `o1`, `o2`, ... , `oN` respectively after the synchronization takes place. 
If no synchronization occurs then an empty message {} is emitted through `o1`, `o2`, ... , `oN` respectively.

