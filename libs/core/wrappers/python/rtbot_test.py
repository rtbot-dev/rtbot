import rtbot.libs.core.wrappers.python.rtbotapi as api
import rtbot.libs.core.wrappers.python.rtbot as rtbot
import json

programId = "1234"
program = """
{
  "title": "Peak detector",
  "description": "This is a program to detect peaks in PPG...",
  "date": "now",
  "apiVersion": "v1",
  "author": "Someone <someone@gmail.com>",
  "license": "MIT",
  "operators": [
    {
      "id": "in1",
      "type": "Input"
    },
    {
      "id": "ma1",
      "type": "MovingAverage",
      "n": 6
    },
    {
      "id": "ma2",
      "type": "MovingAverage",
      "n": 250
    },
    {
      "id": "minus",
      "type": "Minus",
      "policies": { "i1": { "eager": false } }
    },
    {
      "id": "peak",
      "type": "PeakDetector",
      "n": 13
    },
    {
      "id": "join",
      "type": "Join",
      "policies": { "i1": { "eager": false } },
      "numPorts": 2
    },
    {
      "id": "out1",
      "type": "Output",
      "numPorts": 1
    }
  ],
  "connections": [
    {
      "from": "in1",
      "to": "ma1",
      "fromPort": "o1",
      "toPort": "i1"
    },
    {
      "from": "ma1",
      "to": "minus",
      "fromPort": "o1",
      "toPort": "i1"
    },
    {
      "from": "minus",
      "to": "peak",
      "fromPort": "o1",
      "toPort": "i1"
    },
    {
      "from": "peak",
      "to": "join",
      "fromPort": "o1",
      "toPort": "i1"
    },
    {
      "from": "join",
      "to": "out1",
      "fromPort": "o2",
      "toPort": "i1"
    },
    {
      "from": "in1",
      "to": "ma2",
      "fromPort": "o1",
      "toPort": "i1"
    },
    {
      "from": "ma2",
      "to": "minus",
      "fromPort": "o1",
      "toPort": "i2"
    },
    {
      "from": "in1",
      "to": "join",
      "fromPort": "o1",
      "toPort": "i2"
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

program1 = rtbot.Program("test")
con1 = rtbot.Connection("m1", "m2", "o1", "i1")

print(f"{json.dumps(program1, cls = rtbot.ProgramEncoder)}")
print(f"{json.dumps(con1, cls = rtbot.ConnectionEncoder)}")

print(f"Registering program id {programId}")
api.registerProgram(programId, program)

for row in data:
    response = api.sendMessage(programId, row[0], row[1])
    print(f"received {response}")

print(f"Deleting program id {programId}")
api.deleteProgram(programId)
