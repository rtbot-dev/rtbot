from rtbot import rtbotapi as api
import json
import random

from typing import Optional
from json import JSONEncoder

class Run(object):
    def __init__(self, program, data):
        program.validate()
        self.program = program
        self.data = data

    def exec(self):
        api.createProgram(self.program.id, json.dumps(self.program, cls = ProgramEncoder))
        result = dict()

        for row in self.data:
            response = json.loads(api.sendMessage(self.program.id, row[0], row[1]))
            for opId, op in response.items():
                for portId, msgs in op.items():
                    key = f"{opId}:{portId}"
                    if not key in result:
                        result[key] = { 'time': [], 'value': []}
                    for msg in msgs:
                        result[key]['time'].append(int(msg.get("time")))
                        result[key]['value'].append(float(msg.get("value")))

        api.deleteProgram(self.program.id)
        return result


def parse(payload):
    program = Program(**json.loads(payload))

    validation = program.validate()
    if not validation["valid"]:
        raise Exception(validation["error"])

    return program

class Program(object):
    def __init__(self, 
                 operators = [],
                 connections = [],
                 title: Optional[str] = None,
                 description: Optional[str] = None,
                 apiVersion: Optional[str] = "v1",
                 date: Optional[str] = None,
                 author: Optional[str] = None,
                 license: Optional[str] = None,
                 ):
        # print(f"Ininitializing program with operators {operators}")
        self.title = title
        self.description = description
        self.apiVersion = apiVersion
        self.date = date
        self.author = author
        self.license = license
        self.operators = operators
        self.connections = connections
        self.id = f'{random.randrange(16**4):04x}'

    def validate(self):
        return json.loads(api.validate(json.dumps(self, cls = ProgramEncoder)))

    def addOperator(self, op):
        ids = list(map(lambda op: op["id"], self.operators))
        validation = op.validate()
        if not validation["valid"]:
          raise Exception(validation["error"])

        if op["id"] in ids:
          raise Exception(f"Operator with same id {op['id']}, already added to program (maybe you are adding it twice?)")

        self.operators.append(op)

    def addConnection(self, fromOp: str, toOp: str, fromPort: str, toPort: str):
        # check if operators have been added already, raise an exception otherwise
        # print(f"operators {self.operators}")
        ids = list(map(lambda op: op["id"], self.operators))
        if fromOp not in ids:
            raise Exception(f"Operator {fromOp} hasn't been added to the program")
        if toOp not in ids:
            raise Exception(f"Operator {toOp} hasn't been added to the program")

        self.connections.append(Connection(fromOp, toOp, fromPort, toPort))


class ProgramEncoder(JSONEncoder):
    def default(self, o):
        return o.__dict__

class Connection(object):
    def __init__(self, fromOp: str, toOp: str, fromPort: str, toPort: str):
        self.fromOp = fromOp
        self.toOp = toOp
        self.fromPort = fromPort
        self.toPort = toPort

class ConnectionEncoder(JSONEncoder):
    def default(self, o):
        return { "from": o.fromOp, "to": o.toOp, "fromPort": o.fromPort, "toPort": o.toPort }
