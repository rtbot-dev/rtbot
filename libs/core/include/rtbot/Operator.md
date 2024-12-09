# RtBot Operator Documentation

## Overview

The RtBot Operator class serves as the fundamental building block for creating data processing pipelines. It implements a message-passing architecture where operators can receive data and control messages through input ports, process them, and emit results through output ports. The design follows principles similar to FFMPEG filters and Web Audio nodes.

## Key Concepts

### Messages

- Base type: `BaseMessage`
  - Contains a `timestamp_t` (64-bit integer) timestamp
  - Defines virtual `clone()` method for message copying
- Typed messages: `Message<T>`
  - Derives from `BaseMessage`
  - Contains typed data of type `T`
  - Implements deep copying through `clone()`

### Built-in Data Types

- `NumberData`: Holds a single double value
- `BooleanData`: Holds a single boolean value
- `VectorNumberData`: Holds a vector of doubles
- `VectorBooleanData`: Holds a vector of booleans

### Ports

1. Data Ports (`data_ports_`)

   - Primary input ports for data processing
   - Number specified at construction time
   - Can be dynamically added using `add_data_port()`
   - Indexed from 0 to (num_data_ports - 1)

2. Control Ports (`control_ports_`)

   - Secondary input ports for control signals
   - Number specified at construction time
   - Can be dynamically added using `add_control_port()`
   - Indexed from 0 to (num_control_ports - 1)

3. Output Ports (`output_ports_`)
   - Ports for emitting processed data
   - Initially matches number of data ports
   - Can be dynamically added using `add_output_port()`
   - Indexed from 0 to (num_output_ports - 1)

## Constructor

```cpp
Operator(std::string id, size_t num_data_ports = 1, size_t num_control_ports = 0)
```

- `id`: Unique identifier for the operator
- `num_data_ports`: Initial number of data input ports (default: 1)
- `num_control_ports`: Initial number of control input ports (default: 0)

## Core Methods

### Message Reception

```cpp
void receive_data(std::unique_ptr<BaseMessage> msg, size_t port_index)
void receive_control(std::unique_ptr<BaseMessage> msg, size_t port_index)
```

- Receives messages on specified ports
- Takes ownership of the message
- Validates port indices
- Tracks which ports have received new data
- Throws if port index is invalid

### Execution

```cpp
void execute()
```

1. Checks if there are new messages to process
2. Clears previous outputs
3. Processes control messages if any exist
4. Processes data messages
5. Propagates outputs to connected operators
6. Clears tracking of new data

### Connections

```cpp
void connect(Operator* child, size_t output_port = 0, size_t child_input_port = 0)
```

- Creates connection from this operator's output port to child's input port
- Default ports are 0 for simple cases
- Throws if output port index is invalid

## Protected Interface

### Virtual Methods

```cpp
virtual void process_data() = 0
virtual void process_control() {}
```

- `process_data()`: Must be implemented by derived classes
- `process_control()`: Optional, default does nothing

### Helper Methods

```cpp
template<typename T>
const Message<T>* get_message(const BaseMessage* msg) const

template<typename T>
std::unique_ptr<Message<T>> create_message(timestamp_t time, T data)
```

- Type-safe message access and creation
- Dynamic casting for message type checking
- Memory management through unique pointers

### Port Access

```cpp
const MessageQueue& get_data_queue(size_t port_index) const
const MessageQueue& get_control_queue(size_t port_index) const
MessageQueue& get_output_queue(size_t port_index)
```

- Safe access to message queues
- Const and non-const versions as appropriate
- Used by derived classes to implement processing logic

## State Management

```cpp
virtual void restore(Bytes::const_iterator& it) = 0
virtual Bytes collect() = 0
```

- Abstract methods for serialization
- Must be implemented by derived classes
- Used for saving/restoring operator state

## Processing Flow

1. Messages arrive through `receive_data()` or `receive_control()`
2. `execute()` is called (typically by parent or program)
3. Control messages are processed first if any exist
4. Data messages are processed
5. Results are propagated to children
6. Children execute their own processing
7. Process continues through the operator graph

## Thread Safety

The current implementation is not thread-safe. All operations should be performed from the same thread. If thread safety is needed, appropriate synchronization must be added.

## Error Handling

All methods that take port indices validate them and throw `std::runtime_error` if they are invalid. Derived classes should maintain this behavior in their implementations.

## Memory Management

The class uses `std::unique_ptr` throughout to ensure proper resource management:

- Messages are owned by their containing queues
- Message copying is handled through the virtual `clone()` method
- No manual memory management is required

## Extending the Operator

When creating a new operator:

1. Inherit from `Operator`
2. Implement `process_data()`
3. Optionally implement `process_control()`
4. Implement `restore()` and `collect()` for state management
5. Use helper methods for message handling
6. Consider providing a type-safe interface in the derived class

## Example Usage

```cpp
class MyOperator : public Operator {
public:
    MyOperator(std::string id)
        : Operator(std::move(id), 1, 1)  // 1 data port, 1 control port
    {
        add_output_port();  // Add second output port
    }

protected:
    void process_data() override {
        const auto& input = get_data_queue(0);
        if(input.empty()) return;

        // Process messages...
        auto& output = get_output_queue(0);
        output.push_back(create_message<NumberData>(
            input.front()->time,
            NumberData{/* processed value */}
        ));
    }
};
```
