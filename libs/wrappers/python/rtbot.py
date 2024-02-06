from rtbot import rtbotapi as api
import json
import random

from typing import Optional


class Run(object):
    def __init__(self, program, data):
        program.validate()
        self.program = program
        # data is a list of rows
        # the first elemet of a row is the time
        # the other elements are assumend to correspond to
        # the input of the port i according to the `ports`
        # list passed
        self.data = data

    def exec(self):
        result = api.createProgram(self.program.id, self.program.toJson())
        # TODO: here we assume that the data is a list of rows, which are a list
        # of numbers, where the first element of the row is the time and the remaining
        # ones correspond to the values passed to the ports and they have the order according
        # to the one obtained by `getProgramEntryPorts`.
        # Move this to receive a pandas dataframe instead
        ports = json.loads(api.getProgramEntryPorts(self.program.id))

        if result != "":
            raise Exception(json.loads(result)["error"])

        result = {}

        for row in self.data:
            # print(f"Sending {row[0]}, {row[1]}")
            for i in range(1, len(row)):
                api.addToMessageBuffer(self.program.id, ports[i - 1], row[0], row[i])

            response = json.loads(api.processMessageBufferDebug(self.program.id))
            # print(f"response {response}")
            for opId, op in response.items():
                for portId, msgs in op.items():
                    key = f"{opId}:{portId}"
                    if not key in result:
                        result[key] = {"time": [], "value": []}
                    for msg in msgs:
                        result[key]["time"].append(int(msg.get("time")))
                        result[key]["value"].append(float(msg.get("value")))

        api.deleteProgram(self.program.id)
        return result


def parse(payload):
    program = Program(**json.loads(payload))

    validation = program.validate()
    if not validation["valid"]:
        print(program.toJson())
        raise Exception(validation["error"])

    return program


class Program:
    def __init__(
        self,
        operators=None,
        connections=None,
        entryOperator: Optional[str] = None,
        title: Optional[str] = None,
        description: Optional[str] = None,
        apiVersion: Optional[str] = "v1",
        date: Optional[str] = None,
        author: Optional[str] = None,
        license: Optional[str] = None,
    ):
        # print(f"Ininitializing program with operators {operators}, connections {connections}")
        self.title = title
        self.description = description
        self.apiVersion = apiVersion
        self.date = date
        self.author = author
        self.license = license
        self.operators = operators if operators is not None else []
        self.connections = connections if connections is not None else []
        self.entryOperator = entryOperator
        self.id = f"{random.randrange(16**4):04x}"

    def toJson(self):
        obj = {
            "apiVersion": self.apiVersion,
            "operators": self.operators,
            "connections": self.connections,
        }
        if self.title:
            obj["title"] = self.title

        if self.author:
            obj["author"] = self.author

        if self.description:
            obj["description"] = self.description

        if self.license:
            obj["license"] = self.license

        if self.date:
            obj["date"] = self.date

        if self.date:
            obj["entryOperator"] = self.entryOperator

        return json.dumps(obj)

    def validate(self):
        return json.loads(api.validate(self.toJson()))

    def addOperator(self, op):
        ids = list(map(lambda op: op["id"], self.operators))
        validation = op.validate()
        if not validation["valid"]:
            raise Exception(validation["error"])

        if op["id"] in ids:
            raise Exception(
                f"Operator with same id {op['id']}, already added to program (maybe you are adding it twice?)"
            )

        self.operators.append(op)
        return self

    def addConnection(
        self, fromOp: str, toOp: str, fromPort: str = "o1", toPort: str = "i1"
    ):
        # check if operators have been added already, raise an exception otherwise
        ids = list(map(lambda op: op["id"], self.operators))
        if fromOp not in ids:
            raise Exception(f"Operator {fromOp} hasn't been added to the program")
        if toOp not in ids:
            raise Exception(f"Operator {toOp} hasn't been added to the program")

        self.connections.append(Connection(fromOp, toOp, fromPort, toPort))
        return self

    def toMermaidJs(self):
        content = "flowchart LR;"
        for con in self.connections:
            fromOp = next((x for x in self.operators if x["id"] == con["from"]), None)
            toOp = next((x for x in self.operators if x["id"] == con["to"]), None)
            fromType = fromOp["type"]
            toType = toOp["type"]
            args = []
            for k in fromOp.keys():
                if k != "id" and k != "type":
                    args.append(f"{fromOp[k]}")
            if len(args) > 0:
                fromOpArgs = f'({",".join(args)})'
            else:
                fromOpArgs = ""

            args = []
            for k in toOp.keys():
                if k != "id" and k != "type":
                    args.append(f"{toOp[k]}")
            if len(args) > 0:
                toOpArgs = f'({",".join(args)})'
            else:
                toOpArgs = ""

            content += f'{con["from"]}("{fromType}{fromOpArgs}") --> |{con["fromPort"]}:{con["toPort"]}| {con["to"]}("{toType}{toOpArgs}");\n'

        return content


class Connection(dict):
    def __init__(self, fromOp: str, toOp: str, fromPort: str, toPort: str):
        dict.__init__(self)
        self["from"] = fromOp
        self["to"] = toOp
        self["fromPort"] = fromPort
        self["toPort"] = toPort
