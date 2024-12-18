from rtbot import rtbotapi as api
import json
import random
import warnings
import pandas as pd
from typing import Optional, Dict, List, Any, Union

class Run:
    def __init__(self, program, data: pd.DataFrame, port_mappings: Optional[Dict[str, str]] = None, 
                 debug: bool = False):
        program.validate()
        self.program = program
        self.data = data
        self.debug = debug
        self.current_index = 0
        self.program_initialized = False
        
        if 'time' not in data.columns:
            raise ValueError("DataFrame must contain a 'time' column")
            
        self.column_to_port = {}
        if port_mappings:
            for col, port in port_mappings.items():
                if col in data.columns:
                    self.column_to_port[col] = port
                else:
                    warnings.warn(f"Mapped column '{col}' not found in DataFrame")
        else:
            for col in data.columns:
                if col != 'time':
                    self.column_to_port[col] = col
                    warnings.warn(f"No port mapping provided for column '{col}', using column name as port ID")

    def __del__(self):
        if self.program_initialized and not self.debug:
            api.delete_program(self.program.id)

    def exec(self, output_mappings: Optional[Dict[str, str]] = None, next: int = None) -> pd.DataFrame:
        if not self.program_initialized:
            result = api.create_program(self.program.id, self.program.to_json())
            if result != "":
                raise Exception(result)
            self.program_initialized = True

        df_data = {'time': []}
        
        # Initialize columns for all ports regardless of mapping
        if self.debug:
            for operator in self.program.operators:
                op_id = operator["id"]
                num_ports = len(operator.get("portTypes", [1]))
                for port_idx in range(num_ports):
                    col_key = f"{op_id}:o{port_idx + 1}"
                    df_data[col_key] = []

        # Initialize output operator columns
        for op_id, ports in self.program.output.items():
            for port_id in ports:
                key = f"{op_id}:{port_id}"
                col_name = output_mappings.get(key, key) if output_mappings else key
                if key not in df_data:
                    df_data[key] = []

        end_index = min(self.current_index + next, len(self.data)) if next else len(self.data)

        # Process rows
        for idx in range(self.current_index, end_index):
            row = self.data.iloc[idx]
            
            # Add messages to buffer using port mappings
            for col, port_id in self.column_to_port.items():
                value = row[col]
                if pd.notna(value):
                    api.add_to_message_buffer(self.program.id, port_id, int(row['time']), float(value))
            
            # Process messages and collect results
            batch_result = json.loads(api.process_message_buffer_debug(self.program.id) if self.debug 
                                    else api.process_message_buffer(self.program.id))
            
            # Update dataframe with message values
            time_val = int(row['time'])
            df_data['time'].append(time_val)
            
            # Extend all columns to current length with None
            for col in df_data:
                if len(df_data[col]) < len(df_data['time']):
                    df_data[col].append(None)
            
            # Update values from batch results
            for op_id, ports in batch_result.items():
                for port_id, messages in ports.items():
                    col_key = f"{op_id}:{port_id}"
                    if col_key in df_data and messages:
                        # Use the last message value for the current timestamp
                        df_data[col_key][-1] = float(messages[-1]["value"])

        self.current_index = end_index

        if not self.debug and self.current_index >= len(self.data):
            api.delete_program(self.program.id)
            self.program_initialized = False

        return pd.DataFrame(df_data)

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

    def to_mermaid(self) -> str:
        """Convert program structure to Mermaid.js flowchart representation."""
        # Start with flowchart definition, left-to-right
        lines = ["flowchart LR"]
        
        # Create nodes for each operator
        for op in self.operators:
            op_id = op["id"]
            op_type = op["type"]
            
            # Special styling for entry operator
            if op_id == self.entryOperator:
                lines.append(f'    {op_id}["{op_type}\\n{op_id}"]:::entry')
            # Special styling for output operators
            elif op_id in self.output:
                lines.append(f'    {op_id}["{op_type}\\n{op_id}"]:::output')
            else:
                lines.append(f'    {op_id}["{op_type}\\n{op_id}"]')
        
        # Add connections
        for conn in self.connections:
            from_op = conn["from"]
            to_op = conn["to"]
            from_port = conn.get("fromPort", "o1")
            to_port = conn.get("toPort", "i1")
            
            # Add port labels to connection
            lines.append(f'    {from_op} -- "{from_port} → {to_port}" --> {to_op}')
        
        # Add class definitions
        lines.extend([
            "    classDef entry fill:#f96",
            "    classDef output fill:#9cf"
        ])
        
        return "\n".join(lines)

class Connection(dict):
    def __init__(self, from_op: str, to_op: str, from_port: str, to_port: str):
        super().__init__()
        self["from"] = from_op
        self["to"] = to_op
        self["fromPort"] = from_port
        self["toPort"] = to_port
