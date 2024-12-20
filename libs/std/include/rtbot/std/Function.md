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

The Function operator transforms input values through a piecewise function specified by a set of points, using either linear or Hermite interpolation.

Inputs: 1 port (port 0)  
Outputs: 1 port (port 0)

## Operation

For input values between defined points, the operator performs interpolation according to the specified method:

- `linear`: Simple linear interpolation between adjacent points
- `hermite`: Smooth cubic Hermite interpolation using estimated tangents

For input values outside the defined range, the operator extrapolates using the chosen method.

The operator emits transformed values through port 0 immediately after receiving input.

## Example

Given points: [(0,0), (1,2), (2,0)] with linear interpolation:

```
Time  Input  Output  Notes
1     0.5    1.0     Linear interpolation between (0,0) and (1,2)
3     1.5    1.0     Linear interpolation between (1,2) and (2,0)
5     2.5    -1.0    Linear extrapolation beyond (2,0)
```

## Points Specification

Points must be provided as (x,y) coordinate pairs. At least 2 points are required. Points are automatically sorted by x-coordinate.

## Interpolation Types

### Linear

- Simple straight-line interpolation between points
- Computationally efficient
- Continuous but not smooth at control points

### Hermite

- Smooth cubic interpolation using estimated tangents
- Better for representing natural curves
- Maintains continuity of first derivatives
- More computationally intensive

## Use Cases

- Signal value transformation
- Sensor calibration curves
- Transfer function implementation
- Non-linear scaling
- Smooth transition functions
