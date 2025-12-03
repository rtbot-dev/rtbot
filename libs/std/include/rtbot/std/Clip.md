# Clip Operator

The `Clip` operator constrains numeric values to a closed interval. Both lower and upper bounds are optional.

## Parameters
- `lower` (number, optional): Minimum value. If omitted the input is not bounded below.
- `upper` (number, optional): Maximum value. If omitted the input is not bounded above.

## Ports
- Input `NumberData`
- Output `NumberData`

## Behaviour
- Each incoming sample is clamped between `lower` and `upper` if the bounds are provided.
- Bounds may be given independently (only lower or only upper).
