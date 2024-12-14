import sys
import os

sys.path.append(os.getcwd() + "/libs/wrappers/python")

import unittest
import rtbot
from rtbot import rtbotapi as api
import json

class RtBotTest(unittest.TestCase):
    def setUp(self):
        self.program_id = "test_program"
        self.program_json = """{
            "title": "Test Program",
            "apiVersion": "v1",
            "entryOperator": "in1",
            "output": {"out1": ["o1"]},
            "operators": [
                {"id": "in1", "type": "Input", "portTypes": ["number"]},
                {"id": "ma1", "type": "MovingAverage", "window_size": 6}
            ],
            "connections": [
                {"from": "in1", "to": "ma1", "fromPort": "o1", "toPort": "i1"}
            ]
        }"""
        
        self.test_data = [
            [1, 10.0],
            [2, 15.0],
            [3, 20.0]
        ]

    def test_program_creation(self):
        result = api.create_program(self.program_id, self.program_json)
        self.assertEqual(result, "")
        api.delete_program(self.program_id)

    def test_program_validation(self):
        result = json.loads(api.validate_program(self.program_json))
        self.assertTrue(result["valid"])

    def test_message_processing(self):
        api.create_program(self.program_id, self.program_json)
        for t, v in self.test_data:
            api.add_to_message_buffer(self.program_id, "in1", t, v)
        result = json.loads(api.process_message_buffer_debug(self.program_id))
        self.assertIn("in1", result)
        api.delete_program(self.program_id)

    def test_program_class(self):
        program = rtbot.Program(title="test")
        self.assertIsNotNone(program)
        self.assertEqual(program.title, "test")

if __name__ == "__main__":
    unittest.main()