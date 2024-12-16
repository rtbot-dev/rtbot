from rtbot import rtbotapi as api
import json
import random
from typing import Optional, Dict, List, Any, Union

class Run:
    def __init__(self, program, data):
        program.validate()
        self.program = program
        self.data = data

    def exec(self) -> Dict[str, Dict[str, List[Any]]]:
        result = api.create_program(self.program.id, self.program.to_json())
        if result != "":
            raise Exception(result)

        entry_id = api.get_program_entry_operator_id(self.program.id)
        result = {}

        for row in self.data:
            api.add_to_message_buffer(self.program.id, entry_id, row[0], row[1])
            batch_result = json.loads(api.process_message_buffer(self.program.id))
            
            # Process results according to output mapping
            for op_id, ports in batch_result.items():
                if op_id in self.program.output:
                    mapped_ports = self.program.output[op_id]
                    for port_id in mapped_ports:
                        if port_id in ports:
                            key = f"{op_id}:{port_id}"
                            if key not in result:
                                result[key] = {"time": [], "value": []}
                            for msg in ports[port_id]:
                                result[key]["time"].append(int(msg["time"]))
                                result[key]["value"].append(float(msg["value"]))

        api.delete_program(self.program.id)
        return result

class Program:
    def __init__(
        self,
        operators: Optional[List[Dict]] = None,
        connections: Optional[List[Dict]] = None,
        entryOperator: Optional[str] = None,
        output: Optional[Dict[str, Union[List[str], Dict[str, str]]]] = None,
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

    def add_output(self, operator_id: str, ports: List[str]) -> 'Program':
        ids = [op["id"] for op in self.operators]
        if operator_id not in ids:
            raise Exception(f"Operator {operator_id} not found in program")
        
        self.output[operator_id] = ports
        return self

class Connection(dict):
    def __init__(self, from_op: str, to_op: str, from_port: str, to_port: str):
        super().__init__()
        self["from"] = from_op
        self["to"] = to_op
        self["fromPort"] = from_port
        self["toPort"] = to_port
