import sys
import os
import unittest
import json
import pandas as pd
import numpy as np
from typing import List, Dict
import uuid

sys.path.append(os.getcwd() + "/libs/wrappers/python")

import rtbot
from rtbot import rtbotapi as api
from rtbot.operators import MovingAverage, Input, Output

class RtBotTest(unittest.TestCase):
    def setUp(self):
        # Base program JSON that will be used as a template
        self.base_program_json = """{
            "title": "Test Program",
            "apiVersion": "v1",
            "entryOperator": "in1",
            "output": {"out1": ["o1"]},
            "operators": [
                {"id": "in1", "type": "Input", "portTypes": ["number"]},
                {"id": "ma1", "type": "MovingAverage", "window_size": 3},
                {"id": "out1", "type": "Output", "portTypes": ["number"]}
            ],
            "connections": [
                {"from": "in1", "to": "ma1", "fromPort": "o1", "toPort": "i1"},
                {"from": "ma1", "to": "out1", "fromPort": "o1", "toPort": "i1"}
            ]
        }"""
        
        # More test data points to ensure we see MovingAverage output
        self.test_data = [
            [1, 10.0],
            [2, 15.0],
            [3, 20.0],
            [4, 25.0],
            [5, 30.0]
        ]

    def get_unique_program_id(self):
        """Generate a unique program ID for each test"""
        return f"test_program_{str(uuid.uuid4())[:8]}"

    def test_program_creation(self):
        """Test basic program creation and deletion"""
        program_id = self.get_unique_program_id()
        result = api.create_program(program_id, self.base_program_json)
        self.assertEqual(result, "")
        api.delete_program(program_id)

    def test_program_validation(self):
        """Test program JSON validation"""
        result = json.loads(api.validate_program(self.base_program_json))
        self.assertTrue(result["valid"])

        # Test invalid program
        invalid_json = """{
            "title": "Invalid Program",
            "apiVersion": "v1",
            "operators": []
        }"""
        result = json.loads(api.validate_program(invalid_json))
        self.assertFalse(result["valid"])

    def test_message_processing(self):
        """Test basic message processing"""
        program_id = self.get_unique_program_id()
        api.create_program(program_id, self.base_program_json)
        
        # Send all test data points
        for t, v in self.test_data:
            api.add_to_message_buffer(program_id, "i1", t, v)
            
        result = json.loads(api.process_message_buffer_debug(program_id))
        
        # Now we should see both input and MA output
        self.assertIn("in1", result)
        self.assertIn("ma1", result)
        
        api.delete_program(program_id)

    def test_program_class(self):
        """Test Program class construction and methods"""
        program = rtbot.Program(
            title="test",
            description="Test program",
            apiVersion="v1",
            operators=[
                {"id": "in1", "type": "Input", "portTypes": ["number"]},
                {"id": "ma1", "type": "MovingAverage", "window_size": 3},
                {"id": "out1", "type": "Output", "portTypes": ["number"]}
            ],
            connections=[
                {"from": "in1", "to": "ma1", "fromPort": "o1", "toPort": "i1"},
                {"from": "ma1", "to": "out1", "fromPort": "o1", "toPort": "i1"}
            ],
            entryOperator="in1",
            output={"out1": ["o1"]}
        )
        
        validation = program.validate()
        self.assertTrue(validation["valid"], f"Validation failed: {validation.get('error', 'Unknown error')}")

    def test_batch_processing(self):
        """Test batch processing with DataFrame"""
        # Create test data
        df = pd.DataFrame({
            'time': range(1, 11),
            'input_value': np.random.rand(10)
        })
        
        # Create and configure program
        program = rtbot.Program(
            operators=[
                {"id": "in1", "type": "Input", "portTypes": ["number"]},
                {"id": "ma1", "type": "MovingAverage", "window_size": 3},
                {"id": "out1", "type": "Output", "portTypes": ["number"]}
            ],
            connections=[
                {"from": "in1", "to": "ma1", "fromPort": "o1", "toPort": "i1"},
                {"from": "ma1", "to": "out1", "fromPort": "o1", "toPort": "i1"}
            ],
            entryOperator="in1",
            output={"out1": ["o1"]}
        )
        
        # Process data with explicit port mapping
        run = rtbot.Run(program, df, port_mappings={'input_value': 'i1'})
        result_df = run.exec()
        
        # Verify results
        self.assertIsInstance(result_df, pd.DataFrame)
        self.assertIn('time', result_df.columns)
        self.assertEqual(len(result_df), len(df))

    def test_operator_validation(self):
        """Test operator validation"""
        # Test valid operator
        valid_ma = {
            "type": "MovingAverage",
            "id": "ma1",
            "window_size": 5
        }
        validation = json.loads(api.validate_operator("MovingAverage", json.dumps(valid_ma)))
        self.assertTrue(validation["valid"])
        
        # Test invalid operator
        invalid_ma = {"type": "MovingAverage", "id": "ma1"}  # Missing window_size
        validation = json.loads(api.validate_operator("MovingAverage", json.dumps(invalid_ma)))
        self.assertFalse(validation["valid"])

    def test_debug_mode(self):
        """Test debug mode processing"""
        program_id = self.get_unique_program_id()
        api.create_program(program_id, self.base_program_json)
        
        # Process in debug mode with enough data points
        for t, v in self.test_data:
            api.add_to_message_buffer(program_id, "i1", t, v)
            
        debug_result = json.loads(api.process_message_buffer_debug(program_id))
        
        # Verify debug output contains all operator states
        self.assertIn("in1", debug_result)
        self.assertIn("ma1", debug_result)  # Should now appear since we have enough data points
        
        api.delete_program(program_id)

    def test_port_mappings(self):
        """Test custom port mappings"""
        df = pd.DataFrame({
            'time': range(1, 6),
            'sensor1': np.random.rand(5),
            'sensor2': np.random.rand(5)
        })
        
        program = rtbot.Program(
            operators=[
                {"id": "in1", "type": "Input", "portTypes": ["number", "number"]},
                {"id": "ma1", "type": "MovingAverage", "window_size": 3},
                {"id": "out1", "type": "Output", "portTypes": ["number"]}
            ],
            connections=[
                {"from": "in1", "to": "ma1", "fromPort": "o1", "toPort": "i1"},
                {"from": "ma1", "to": "out1", "fromPort": "o1", "toPort": "i1"}
            ],
            entryOperator="in1",
            output={"out1": ["o1"]}
        )
        
        # Test with explicit port mappings for both sensors
        run = rtbot.Run(program, df, port_mappings={
            'sensor1': 'i1',
            'sensor2': 'i2'
        })
        result = run.exec()
        self.assertIsInstance(result, pd.DataFrame)

    def test_multiple_input_ports(self):
        """Test program with multiple input ports"""
        program_id = self.get_unique_program_id()
        
        # Create a program with multiple input ports
        program_json = """{
            "title": "Multi-Input Test",
            "apiVersion": "v1",
            "entryOperator": "in1",
            "output": {"out1": ["o1"]},
            "operators": [
                {"id": "in1", "type": "Input", "portTypes": ["number", "number"]},
                {"id": "out1", "type": "Output", "portTypes": ["number"]}
            ],
            "connections": [
                {"from": "in1", "to": "out1", "fromPort": "o1", "toPort": "i1"}
            ]
        }"""
        
        api.create_program(program_id, program_json)
        
        # Test sending messages to different input ports
        api.add_to_message_buffer(program_id, "i1", 1, 10.0)
        api.add_to_message_buffer(program_id, "i2", 1, 20.0)
        
        result = json.loads(api.process_message_buffer_debug(program_id))
        self.assertIn("in1", result)
        
        api.delete_program(program_id)

if __name__ == "__main__":
    unittest.main()