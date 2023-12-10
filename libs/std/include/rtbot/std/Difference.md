---
behavior:
  buffered: true
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \Delta
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
  required: ["id"]
---

# Difference

Inputs: `i1`  
Outputs: `o1`

Computes the difference between the values of two consecutive messages. Emits the difference using the newest message time field. 

The `Difference` operator holds a message buffer of size 2 on `i1`. Once the message buffer on `i1` gets filled it calculates the difference of the newest message and the oldest message from the buffer and it emits a new message with the calculated difference and the time of the newest message.

$$y(t_n)=x(t_n) - x(t_{n-1})$$