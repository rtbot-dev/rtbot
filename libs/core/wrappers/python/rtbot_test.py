import sys
import os

sys.path.append(os.getcwd() + "/libs/core/wrappers/python")

import rtbot
from rtbot import rtbotapi as api
from rtbot import operators as op
import json

import unittest

programId = "1234"
program = """
{
  "title": "Peak detector",
  "description": "This is a program to detect peaks in PPG...",
  "apiVersion": "v1",
  "date": "2000-01-01",
  "license": "MIT",
  "author": "Someone <someone@gmail.com>",
  "operators": [
    {
      "id": "in1",
      "type": "Input"
    },
    {
      "id": "ma1",
      "type": "MovingAverage",
      "n": 6
    }
  ],
  "connections": [
    {
      "from": "in1",
      "to": "ma1",
      "fromPort": "o1",
      "toPort": "i1"
    } 
  ]
}
"""

data = [
    [51,8.399677745092632],
    [52,15.079561274193882],
    [53,20.28553118493906],
    [54,24.181306190251043],
    [55,26.930605003052673],
    [56,28.69714633626681],
    [57,29.64464890281631],
    [58,29.93683141562402],
    [59,29.7374125876128],
    [60,29.210111131705506],
    [61,28.51864576082498],
    [62,27.826735187894098]
]

class RtBotTest(unittest.TestCase):
    """RtBot api test"""

    def test_create_valid_program(self):
        """Valid json program can be created"""
        result = api.createProgram(programId, program)
        self.assertEqual(result, "")

    def test_create_invalid_program(self):
        """Invalid json program cannot be created"""
        result = json.loads(api.createProgram(programId, """{}"""))
        self.assertIsNotNone(result["error"])

    def test_delete_valid_program(self):
        """Program created can be deleted"""
        api.createProgram(programId, program)
        result = api.deleteProgram(programId)
        self.assertEqual(result, "")

    def test_validate_valid_program(self):
        """Valid json program is validated correctly"""
        result = json.loads(api.validate(program))
        self.assertTrue(result["valid"])

    def test_invalidate_valid_program(self):
        """Invalid json program is validated correctly"""
        result = json.loads(api.validate("""{}"""))
        self.assertFalse(result["valid"])
        self.assertIsNotNone(result["error"])

    def test_send_message_to_program(self):
        """Messages can be sent to program"""
        api.createProgram(programId, program)
        response = ""
        for row in data:
            response = api.sendMessage(programId, row[0], row[1])
        api.deleteProgram(programId)
        self.assertIsNotNone(json.loads(response)["in1"])

    def test_create_program_class(self):
        """Program can be created through the helper class"""
        program1 = rtbot.Program(title = "test")

    def test_able_to_add_good_connection(self):
        """Able to add a connection if operators referred have been added already to the program"""
        program1 = rtbot.Program(title = "test")
        ma1 = op.MovingAverage("ma1", 2)
        ma2 = op.MovingAverage("ma2", 2)
        program1.addOperator(ma1)
        program1.addOperator(ma2)
        program1.addConnection("ma1", "ma2", "o1", "i1")

    def test_unable_to_add_bad_connection(self):
        """Unable to add a connection if operators referred haven't been added yet to the program"""
        program1 = rtbot.Program(title = "test")
        self.assertRaises(Exception, lambda: program1.addConnection("ma11", "ma12", "o1", "i1"))

    def test_unable_to_add_same_operator_twice(self):
        """Unable to add a connection if operators referred haven't been added yet to the program"""
        program1 = rtbot.Program(title = "test")
        ma1 = op.MovingAverage("ma1", 2)
        program1.addOperator(ma1)
        self.assertRaises(Exception, lambda: program1.addOperator(ma1))

    def test_load_ill_defined_json_program(self):
        """Load ill defined json program it's safely parsed"""
        # it should not throw
        program1 = rtbot.parse("""{ "operators": [] }""")

    def test_load_well_defined_json_program_succeed(self):
        """Load well defined json program succeeds"""
        # it should not throw
        program1 = rtbot.parse(program)

    def test_run_well_defined_json_program_works(self):
        """Run well defined json program works"""
        program1 = rtbot.parse(program)
        result = rtbot.Run(program1, data).exec()
        self.assertTrue("in1:o1" in result)


if __name__ == "__main__":
    unittest.main()


