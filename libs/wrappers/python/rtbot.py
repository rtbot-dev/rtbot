from rtbot import rtbotapi as api
import json
import random
import warnings
import pandas as pd
from typing import Optional, Dict, List, Any, Union

class Run:
    def __init__(self, program, data: pd.DataFrame, port_mappings: Optional[Dict[str, str]] = None):
        """
        Initialize a Run instance
        
        Args:
            program: RTBot program instance
            data: pandas DataFrame containing time and value columns
            port_mappings: Dictionary mapping DataFrame column names to program port IDs
        """
        program.validate()
        self.program = program
        self.data = data
        self.port_mappings = port_mappings or {}
        
        # Verify time column exists
        if 'time' not in data.columns:
            raise ValueError("DataFrame must contain a 'time' column")
            
        # Map columns to ports
        self.column_to_port = {}
        value_columns = [col for col in data.columns if col != 'time']
        
        for col in value_columns:
            if col in self.port_mappings:
                self.column_to_port[col] = self.port_mappings[col]
            else:
                self.column_to_port[col] = col
                warnings.warn(f"No port mapping provided for column '{col}', using column name as port ID")

    def exec(self) -> Dict[str, Dict[str, list]]:
        """Execute the program with the provided data"""
        result = api.create_program(self.program.id, self.program.to_json())
        if result != "":
            raise Exception(result)

        try:
            results = {}
            
            # Process each row
            for _, row in self.data.iterrows():
                # Send data for each value column
                for col, port_id in self.column_to_port.items():
                    value = row[col]
                    if pd.notna(value):  # Only send non-null values
                        api.add_to_message_buffer(self.program.id, port_id, int(row['time']), float(value))
                
                # Process messages and collect results
                batch_result = json.loads(api.process_message_buffer(self.program.id))
                
                # Aggregate results according to output mapping
                for op_id, ports in batch_result.items():
                    if op_id in self.program.output:
                        mapped_ports = self.program.output[op_id]
                        for port_id in mapped_ports:
                            if port_id in ports:
                                key = f"{op_id}:{port_id}"
                                if key not in results:
                                    results[key] = {"time": [], "value": []}
                                for msg in ports[port_id]:
                                    results[key]["time"].append(int(msg["time"]))
                                    results[key]["value"].append(float(msg["value"]))
            
            return results
            
        finally:
            api.delete_program(self.program.id)

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
