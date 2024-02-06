---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      sort
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    numInputs:
      type: integer
      description: "The number of input ports the operator will have. If equal to `N`,
        the operator will have `i1`, `i2`, ..., `iN` as input ports."
      minimum: 2
      examples: [3, 10, 33]
    numOutputs:
      type: integer
      description: "The number of output ports the operator will have. If equal to `N`,
        the operator will have `o1`, `o2`, ..., `oM` as output ports. The
        number of output ports should be smaller than the number of input
        ports, otherwise a runtime exception will be thrown."
      minimum: 1
      examples: [1]
    ascending:
      type: boolean
      description: Whether the indices are sort ascending according to the value in the input messages or not.
      default: true
      examples: [true]
    maxInputBufferSize:
      type: integer
      description: "The maximum length of the internal input buffers that will hold the incoming data
        until synchronization occurs."
      default: 100
      minimum: 1
      examples: [100, 200, 1000]
  required: ["id", "numInputs", "numOutputs"]
---

# SortIndex

Inputs: `i1`...`iN` where N defined by `numInputs`  
Outputs: `o1`...`oM` where M defined by `numOutputs` and $M < N$

Takes $n$ input streams through the input ports `i1`, `i2`, ..., `iN`, and emits the _indices_ (1, 2, ..., M) of the input messages, whenever a synchronization happens, through the ports `o1`, `o2`, ..., `oM` ($M < N$), _after ordering_ them according to the `value` in the messages for _every_ synchronization time. In other words for a given $t$ through the port `o1` will go the _index_ of smallest messages of all input messages, through `o2` the _index_ of the second smallest and so on. Using the `ascending` parameter it is possible to invert the order. For example, if the smallest message `value` is in the input port `i3`, then the output port `o1` will emit the value 3.

Here index refers to the numeric suffix associated to the port. Port `i1` has index 1, port `i2` index 2 and so on.

The synchronization mechanism is inherited from the [`Join`](/docs/operators/core/Join) operator.
