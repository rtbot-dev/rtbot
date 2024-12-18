# Program Prototypes

## Overview

Program Prototypes introduce a powerful abstraction mechanism for creating reusable operator patterns within RtBot programs. Similar to how functions enable code reuse in programming languages, prototypes allow you to define parameterized templates of operator meshes that can be instantiated multiple times with different configurations.

## Motivation

When building complex data processing pipelines, certain combinations of operators often appear repeatedly with slight variations in their configuration. For example:

- Signal threshold detection with counting
- Moving average followed by standard deviation calculation
- Multi-stage filtering chains
- Custom indicator calculations

Without prototypes, implementing these patterns requires duplicating operator definitions and connections, leading to:

- Verbose and repetitive program definitions
- Higher maintenance burden when patterns need to be modified
- Increased chance of configuration errors
- Reduced readability and understanding of program structure

Prototypes solve these challenges by enabling:

- Definition of reusable operator patterns
- Parameterization of pattern configuration
- Clear documentation of intended usage
- Simplified program composition
- Better organized and more maintainable programs

## API Specification

### Prototype Definition

```yaml
prototypes:
  prototypeId:
    parameters:
      - name: paramName
        type: number|string|boolean
        default: value # Optional
    operators:
      - id: opId
        type: OperatorType
        # Operator configuration using ${paramName} syntax
    connections:
      - from: sourceOpId
        to: targetOpId
        fromPort: port # Optional
        toPort: port # Optional
    entry:
      operator: entryOpId
      port: portId
    output:
      operator: outputOpId
      port: portId
```

### Prototype Usage

```yaml
operators:
  - id: instanceId
    prototype: prototypeId
    parameters:
      paramName: value
```

## Parameter Resolution

Parameters defined in the prototype can be referenced in operator configurations using the `${paramName}` syntax. During prototype instantiation:

1. Parameter values provided in the instance configuration are validated against parameter types
2. Default values are applied for any unspecified optional parameters
3. Parameter references are replaced with actual values
4. Operator IDs are made unique by prefixing with the instance ID

## Examples

### Example 1: Signal Threshold Counter

```yaml
prototypes:
  thresholdCounter:
    parameters:
      - name: threshold
        type: number
      - name: window
        type: number
        default: 10
    operators:
      - id: ma
        type: MovingAverage
        window_size: ${window}
      - id: gt
        type: GreaterThan
        value: ${threshold}
      - id: count
        type: CountNumber
    connections:
      - from: ma
        to: gt
      - from: gt
        to: count
    entry:
      operator: ma
      port: i1
    output:
      operator: count
      port: o1

operators:
  - id: highVolume
    prototype: thresholdCounter
    parameters:
      threshold: 1000
      window: 20
  - id: extremeVolume
    prototype: thresholdCounter
    parameters:
      threshold: 5000
```

### Example 2: Bollinger Bands

```yaml
prototypes:
  bollingerBand:
    parameters:
      - name: period
        type: number
        default: 20
      - name: deviations
        type: number
        default: 2
    operators:
      - id: ma
        type: MovingAverage
        window_size: ${period}
      - id: std
        type: StandardDeviation
        window_size: ${period}
      - id: scale
        type: Scale
        value: ${deviations}
      - id: upper
        type: Addition
      - id: lower
        type: Subtraction
    connections:
      - from: ma
        to: upper
      - from: ma
        to: lower
      - from: std
        to: scale
      - from: scale
        to: upper
      - from: scale
        to: lower
    entry:
      operator: ma
      port: i1
    output:
      operator: ma
      port: o1

operators:
  - id: bbands1
    prototype: bollingerBand
    parameters:
      period: 14
      deviations: 2.5
```

## Best Practices

1. **Parameter Naming**

   - Use descriptive parameter names that indicate their purpose
   - Follow a consistent naming convention
   - Document any constraints or expected ranges

2. **Prototype Organization**

   - Group related prototypes together
   - Consider creating a library of common patterns
   - Document the intended use case for each prototype

3. **Pattern Design**

   - Keep prototypes focused on a single responsibility
   - Make parameters optional when reasonable defaults exist
   - Consider the reusability of the pattern in different contexts

4. **Documentation**
   - Describe the purpose and behavior of each prototype
   - Document parameter effects on the prototype's behavior
   - Provide examples of common usage patterns

## Limitations and Considerations

1. **Parameter Types**

   - Parameters are currently limited to simple types (number, string, boolean)
   - Complex types like arrays or objects are not supported

2. **Scope**

   - Parameters can only be used within operator configurations
   - Dynamic connection patterns are not supported

3. **Naming**

   - Instance IDs must be unique within the program
   - Generated operator IDs must not conflict

4. **Validation**
   - Parameter types are validated at instantiation time
   - Missing required parameters will cause an error
   - Invalid parameter values will cause an error

## Future Enhancements

Potential future enhancements to the prototype system may include:

- Support for nested prototypes
- More complex parameter types
