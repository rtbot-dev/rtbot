from rtbot import rtbotapi as api
import json
import random
from typing import Optional, Dict, List, Any

class Run:
    def __init__(self, program, data):
        program.validate()
        self.program = program
        self.data = data

    def exec(self) -> Dict[str, Dict[str, List[Any]]]:
        result = api.create_program(self.program.id, self.program.to_json())
        entry_id = api.get_program_entry_operator_id(self.program.id)

        if result != "":
            raise Exception(result)

        result = {}
        for row in self.data:
            api.add_to_message_buffer(self.program.id, entry_id, row[0], row[1])
            response = json.loads(api.process_message_buffer_debug(self.program.id))
            
            for op_id, op in response.items():
                for port_id, msgs in op.items():
                    key = f"{op_id}:{port_id}"
                    if key not in result:
                        result[key] = {"time": [], "value": []}
                    for msg in msgs:
                        result[key]["time"].append(int(msg["time"]))
                        result[key]["value"].append(float(msg["value"]))

        api.delete_program(self.program.id)
        return result

def parse(payload: str) -> 'Program':
    program = Program(**json.loads(payload))
    validation = json.loads(api.validate_program(program.to_json()))
    if not validation["valid"]:
        raise Exception(validation["error"])
    return program

class Program:
    def __init__(
        self,
        operators: Optional[List[Dict]] = None,
        connections: Optional[List[Dict]] = None,
        entryOperator: Optional[str] = None,
        output: Optional[Dict[str, List[str]]] = None,
        title: Optional[str] = None,
        description: Optional[str] = None,
        apiVersion: Optional[str] = "v1",
        date: Optional[str] = None,
        author: Optional[str] = None,
        license: Optional[str] = None,
    ):
        self.title = title
        self.description = description
        self.apiVersion = apiVersion
        self.date = date
        self.author = author
        self.license = license
        self.operators = operators or []
        self.connections = connections or []
        self.entryOperator = entryOperator
        self.output = output or {}
        self.id = f"{random.randrange(16**4):04x}"

    def to_json(self) -> str:
        obj = {
            "apiVersion": self.apiVersion,
            "operators": self.operators,
            "connections": self.connections,
            "entryOperator": self.entryOperator,
            "output": self.output
        }
        
        for field in ["title", "author", "description", "license", "date"]:
            if getattr(self, field):
                obj[field] = getattr(self, field)

        return json.dumps(obj)

    def validate(self) -> Dict:
        return json.loads(api.validate_program(self.to_json()))

    def add_operator(self, op: Dict) -> 'Program':
        ids = [op["id"] for op in self.operators]
        validation = json.loads(api.validate_operator(op["type"], json.dumps(op)))
        
        if not validation["valid"]:
            raise Exception(validation["error"])
        if op["id"] in ids:
            raise Exception(f"Operator with ID {op['id']} already exists in program")

        self.operators.append(op)
        return self

    def add_connection(self, from_op: str, to_op: str, from_port: str = "o1", to_port: str = "i1") -> 'Program':
        ids = [op["id"] for op in self.operators]
        if from_op not in ids:
            raise Exception(f"Operator {from_op} not found in program")
        if to_op not in ids:
            raise Exception(f"Operator {to_op} not found in program")

        self.connections.append(Connection(from_op, to_op, from_port, to_port))
        return self

class Connection(dict):
    def __init__(self, from_op: str, to_op: str, from_port: str, to_port: str):
        super().__init__()
        self["from"] = from_op
        self["to"] = to_op
        self["fromPort"] = from_port
        self["toPort"] = to_port