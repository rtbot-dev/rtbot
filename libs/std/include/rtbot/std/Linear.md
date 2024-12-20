---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      Linear({{coefficients}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    coefficients:
      type: array
      description: Array of coefficients for the linear combination
      minItems: 2
      items:
        type: number
  required: ["id", "coefficients"]
---

# Linear

A Linear operator computes a linear combination of synchronized input values using predefined coefficients. It extends Join to ensure input synchronization before computing the combination.

## Coefficients

Coefficients are specified during construction and define the linear combination:

- Minimum of 2 coefficients required
- Each coefficient corresponds to one input port
- Output = c₁x₁ + c₂x₂ + ... + cₙxₙ where cᵢ are coefficients and xᵢ are inputs

## Ports

- Input ports: One per coefficient, all NumberData type
- Output port: One NumberData port containing linear combination

## Operation

1. Inherits Join synchronization behavior
2. When inputs are synchronized:
   - Computes linear combination
   - Outputs result on first port
   - Clears other output ports

## Example

```cpp
// Create linear combination: 2x - y
auto linear = make_linear("linear1", {2.0, -1.0});

// Process synchronized inputs
linear->receive_data(create_message<NumberData>(1, NumberData{3.0}), 0); // x = 3
linear->receive_data(create_message<NumberData>(1, NumberData{1.0}), 1); // y = 1
linear->execute();
// Output: 5.0 (2*3 - 1)
```

## Error Handling

Throws exceptions for:

- Fewer than 2 coefficients
- Invalid message types
- Port index out of range

## Performance Considerations

- O(n) computation where n is number of coefficients
- Memory usage proportional to coefficient count
- Numerical stability maintained for large coefficients
