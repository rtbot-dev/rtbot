# RtBot Python Package

Python bindings for [RTBot](https://github.com/rtbot-dev/rtbot), a high-performance stream processing framework.

## Installation

```bash
pip install git+https://github.com/rtbot-dev/rtbot.git#subdirectory=libs/wrappers/python
```

## Usage

```python
import rtbot

# Create a program with a moving average operator
program = rtbot.Program(
    operators=[
        {"type": "Input", "id": "input1", "portTypes": ["number"]},
        {"type": "MovingAverage", "id": "ma1", "window_size": 5},
        {"type": "Output", "id": "output1", "portTypes": ["number"]}
    ],
    connections=[
        {"from": "input1", "to": "ma1", "fromPort": "o1", "toPort": "i1"},
        {"from": "ma1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
    ],
    entryOperator="input1",
    output={"output1": ["o1"]}
)

# Process data
data = [(i, float(i)) for i in range(10)]  # (timestamp, value) pairs
results = rtbot.Run(program, data).exec()

# Access results by operator and port
ma_output = results["ma1:o1"]["value"]
```

## Building from Source

Requirements:

- Python 3.7+
- Git
- Curl

The package will automatically:

1. Download Bazelisk (Bazel build system)
2. Clone the RTBot repository
3. Build the native extension for your platform

## Platform Support

- Linux (x86_64)
- macOS (Intel and Apple Silicon)
- Windows (x86_64)

## Documentation

For complete documentation, visit [RtBot Docs](https://rtbot.dev).
