---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      f(x)
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    points:
      type: array
      description: Array of (x,y) coordinates defining the function
      minItems: 2
      items:
        type: array
        minItems: 2
        maxItems: 2
        items:
          type: number
    type:
      type: string
      enum: ["linear", "hermite"]
      default: "linear"
      description: Interpolation method to use
  required: ["id", "points"]
---

# Function

Inputs: `i1`  
Outputs: `o1`

The `Function` operator transforms input values through a user-defined function specified by a set of points. It supports both linear and Hermite interpolation methods.

For input values between defined points, the operator performs interpolation according to the specified method:

- `linear`: Simple linear interpolation between adjacent points
- `hermite`: Smooth cubic Hermite interpolation using estimated tangents at each point

For input values outside the defined range, the operator extrapolates using the same method.

The `Function` operator does not hold a message buffer on `i1`. It emits transformed values through `o1` immediately after receiving input.

Example usage for linear interpolation between points (0,0) and (1,1):

```cpp
vector<pair<double, double>> points = {{0.0, 0.0}, {1.0, 1.0}};
auto func = Function<uint64_t, double>("func", points, "linear");
```
