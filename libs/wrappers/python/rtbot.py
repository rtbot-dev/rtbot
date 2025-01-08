from rtbot import rtbotapi as api
import json
import random
import warnings
import pandas as pd
import numpy as np
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
            # Handle both regular operators and prototype instances
            for operator in self.program.operators:
                op_id = operator["id"]
                if "prototype" in operator:
                    # For prototype instances, get the prototype definition
                    proto_def = self.program.prototypes[operator["prototype"]]
                    # Track all operators within the prototype
                    for proto_op in proto_def["operators"]:
                        internal_op_id = f"{op_id}::{proto_op['id']}"
                        num_ports = len(proto_op.get("portTypes", [1]))
                        for port_idx in range(num_ports):
                            col_key = f"{internal_op_id}:o{port_idx + 1}"
                            df_data[col_key] = []
                else:
                    # Regular operator handling
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
                    df_data[col].append(np.nan)
            
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
        prototypes: Optional[Dict[str, Dict]] = None,
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
        self.prototypes = prototypes or {}
        self.id = f"{random.randrange(16**4):04x}"

    def to_json(self) -> str:
        obj = {
            "apiVersion": self.apiVersion,
            "operators": self.operators,
            "connections": self.connections,
            "entryOperator": self.entryOperator,
            "output": self.output
        }
        
        if self.prototypes:
            obj["prototypes"] = self.prototypes

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
    def add_prototype(self, prototype_id: str, prototype_def: Dict) -> 'Program':
        """Add a prototype definition to the program.
        
        Args:
            prototype_id: Unique identifier for the prototype
            prototype_def: Dictionary containing prototype definition including:
                - parameters: List of parameter definitions
                - operators: List of operator definitions
                - connections: List of connections
                - entry: Entry operator configuration
                - output: Output operator configuration
        """
        required_keys = ["parameters", "operators", "connections", "entry", "output"]
        missing_keys = [key for key in required_keys if key not in prototype_def]
        if missing_keys:
            raise ValueError(f"Prototype definition missing required keys: {missing_keys}")

        self.prototypes[prototype_id] = prototype_def
        return self

    def instantiate_prototype(self, instance_id: str, prototype_id: str, parameters: Dict[str, Any] = None) -> 'Program':
        """Create an instance of a prototype.
        
        Args:
            instance_id: Unique identifier for the prototype instance
            prototype_id: ID of the prototype to instantiate
            parameters: Dictionary of parameter values for the prototype
        """
        if prototype_id not in self.prototypes:
            raise ValueError(f"Prototype '{prototype_id}' not found")
        
        proto_def = self.prototypes[prototype_id]
        params = parameters or {}

        # Validate required parameters are provided
        required_params = [p["name"] for p in proto_def["parameters"] if "default" not in p]
        missing_params = [p for p in required_params if p not in params]
        if missing_params:
            raise ValueError(f"Missing required parameters for prototype '{prototype_id}': {missing_params}")

        # Apply default values
        for param in proto_def["parameters"]:
            if param["name"] not in params and "default" in param:
                params[param["name"]] = param["default"]

        # Create operator instance with resolved parameters
        instance = {
            "id": instance_id,
            "prototype": prototype_id,
            "parameters": params
        }
        
        self.operators.append(instance)
        return self

    def to_mermaid(self) -> str:
        """Convert program structure to Mermaid.js flowchart representation with improved layout."""
        lines = ["flowchart LR"]
        nodes = {}
        rank_groups = {}
        
        def get_short_name(op_type: str, params: dict = None, op_id: str = "") -> str:
            """Get shortened operator name with parameters."""
            if isinstance(op_type, dict):
                if "prototype" in op_type:
                    proto_name = op_type["prototype"]
                    params = op_type.get("parameters", {})
                    param_str = ", ".join(f"{k}={v}" for k, v in params.items())
                    return f"{proto_name}\\n({param_str})"
                elif "type" in op_type:
                    return get_short_name(op_type["type"], op_type.get("parameters", {}), op_id)
                else:
                    # If we can't determine the type, use the operator's ID
                    return op_id.split("::")[-1]
                
            type_map = {
                "LogicalAnd": "AND",
                "LogicalOr": "OR",
                "LogicalNot": "NOT",
                "MovingAverage": "MA",
                "StandardDeviation": "StdDev",
                "GreaterThan": ">",
                "LessThan": "<",
                "EqualTo": "=",
                "ResamplerHermite": "Hermite",
                "ResamplerConstant": "Resampler",
                "ConstantNumber": "",  # Will show just the value
                "ConstantBoolean": "",  # Will show just the value
                "ConstantNumberToBoolean": "→bool",
                "ConstantBooleanToNumber": "→num",
            }
            
            # Get base name, fallback to operator ID if type unknown
            base_name = type_map.get(op_type, op_type)
            if base_name not in type_map.values() and op_id:
                base_name = op_id.split("::")[-1]
            
            # Add parameters if available
            if params:
                if op_type == "GreaterThan":
                    return f"> {params.get('value', '')}"
                elif op_type == "LessThan":
                    return f"< {params.get('value', '')}"
                elif op_type == "EqualTo":
                    return f"= {params.get('value', '')}"
                elif op_type == "ConstantNumber":
                    return str(params.get('value', ''))
                elif op_type == "ConstantBoolean":
                    return str(params.get('value', '')).lower()
                elif op_type == "MovingAverage":
                    return f"MA({params.get('window_size', '')})"
                elif op_type == "ResamplerHermite":
                    return f"Hermite({params.get('interval', '')})"
                elif op_type == "StandardDeviation":
                    return f"StdDev({params.get('window_size', '')})"
                else:
                    # For any other operator type, show all parameters
                    param_str = ", ".join(f"{k}={v}" for k, v in params.items())
                    return f"{base_name}({param_str})"
            
            return base_name
        
        def add_node(op_id: str, op_type: str, level: int = 0):
            styles = []
            if op_id == self.entryOperator:
                styles.append("entry")
            if op_id in self.output:
                styles.append("output")
            if isinstance(op_type, dict) and "prototype" in op_type:
                styles.append("prototype")
                
            style = ":::" + ",".join(styles) if styles else ""
            
            node_id = op_id.replace("::", "_")
            nodes[op_id] = node_id
            
            if level not in rank_groups:
                rank_groups[level] = []
            rank_groups[level].append(node_id)
            
            # Get parameters and create node label
            params = op_type.get("parameters", {}) if isinstance(op_type, dict) else None
            op_name = get_short_name(op_type if isinstance(op_type, str) else op_type.get("type", "Unknown"), 
                                params, op_id)
            
            # Use hexagon shape for prototype instances
            shape = "{{" if isinstance(op_type, dict) and "prototype" in op_type else "["
            end_shape = "}}" if isinstance(op_type, dict) and "prototype" in op_type else "]"
            
            display_name = op_name if op_name else op_id.split("::")[-1]
            lines.append(f'    {node_id}{shape}"{display_name}"{end_shape}{style}')
            
            # Handle prototypes and pipelines
            if isinstance(op_type, dict) and "prototype" in op_type:
                proto_name = op_type["prototype"]
                proto_def = self.prototypes[proto_name]
                # Add prototype internals
                for internal_op in proto_def["operators"]:
                    internal_id = f"{op_id}::{internal_op['id']}"
                    internal_type = internal_op
                    if isinstance(internal_op, dict):
                        # Resolve template parameters
                        params = {}
                        for k, v in internal_op.get("parameters", {}).items():
                            if isinstance(v, str) and v.startswith("${") and v.endswith("}"):
                                param_name = v[2:-1]
                                if param_name in op_type.get("parameters", {}):
                                    params[k] = op_type["parameters"][param_name]
                            else:
                                params[k] = v
                        internal_type = {"type": internal_op["type"], "parameters": params}
                    add_node(internal_id, internal_type, level + 1)
                    
                # Add prototype connections
                for conn in proto_def["connections"]:
                    from_id = f"{op_id}::{conn['from']}"
                    to_id = f"{op_id}::{conn['to']}"
                    add_connection(from_id, to_id, conn.get("fromPort", "o1"), conn.get("toPort", "i1"))
                    
            elif isinstance(op_type, dict) and op_type.get("type") == "Pipeline":
                # Add pipeline internals
                for internal_op in op_type["operators"]:
                    internal_id = f"{op_id}::{internal_op['id']}"
                    add_node(internal_id, internal_op["type"], level + 1)
                # Add pipeline connections
                for conn in op_type["connections"]:
                    from_id = f"{op_id}::{conn['from']}"
                    to_id = f"{op_id}::{conn['to']}"
                    add_connection(from_id, to_id, conn.get("fromPort", "o1"), conn.get("toPort", "i1"))

        def add_connection(from_op: str, to_op: str, from_port: str = "o1", to_port: str = "i1"):
            from_node = nodes[from_op]
            to_node = nodes[to_op]
            lines.append(f'    {from_node} -- "{from_port} → {to_port}" --> {to_node}')

        # First pass: create all nodes with proper levels
        for op in self.operators:
            add_node(op["id"], op.get("type", op), 0)
        
        # Second pass: add connections
        for conn in self.connections:
            add_connection(conn["from"], conn["to"], 
                        conn.get("fromPort", "o1"), 
                        conn.get("toPort", "i1"))
        
        # Add subgraph rankings to enforce left-to-right layout
        max_level = max(rank_groups.keys()) if rank_groups else 0
        for level in range(max_level + 1):
            if level in rank_groups and rank_groups[level]:
                lines.append(f"    subgraph level_{level} [\" \"]")
                lines.append("    direction LR")  # Force left-to-right direction within subgraph
                lines.append("    " + " & ".join(rank_groups[level]))
                lines.append("    end")
                
        # Add invisible edges to force ordering between levels
        for level in range(max_level):
            if level in rank_groups and (level + 1) in rank_groups:
                if rank_groups[level] and rank_groups[level + 1]:
                    first_node = rank_groups[level][0]
                    next_node = rank_groups[level + 1][0]
                    lines.append(f"    {first_node} ~~~ {next_node}")  # Invisible edge
        
        # Add styling
        lines.extend([
            "    classDef entry fill:#f96,stroke:#333,stroke-width:2px",
            "    classDef output fill:#9cf,stroke:#333,stroke-width:2px",
            "    classDef prototype fill:#f0f0f0,stroke:#666,stroke-width:2px,stroke-dasharray: 5 5",
            "    classDef default fill:white,stroke:#333,stroke-width:1px"
        ])
        
        return "\n".join(lines)

class Connection(dict):
    def __init__(self, from_op: str, to_op: str, from_port: str, to_port: str):
        super().__init__()
        self["from"] = from_op
        self["to"] = to_op
        self["fromPort"] = from_port
        self["toPort"] = to_port
