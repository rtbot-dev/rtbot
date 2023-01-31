use jsonschema::{JSONSchema, ValidationError};
use serde_json::{json, Value};
use std::error::Error;
use std::slice;

pub struct ProgramJsonSchemaValidator {
    schema: Value,
}

impl ProgramJsonSchemaValidator {
    pub fn new() -> Self {
        Self {
            schema: json!({
              "$schema": "http://json-schema.org/draft-07/schema#",
              "type": "object",
              "properties": {
                "title": {
                  "type": "string",
                  "examples": ["Peak detector"]
                },
                "description": {
                  "type": "string",
                  "examples": ["This is a program to detect peaks in ECG..."]
                },
                // "date": {
                //   "type": "date-time"
                // },
                "version": {
                  "type": "string",
                  "examples": ["v1"]
                },
                "author": {
                  "type": "string",
                  "examples": ["Someone <someone@gmail.com>"]
                },
                "license": {
                  "type": "string",
                  "examples": ["MIT", "private"]
                },
                "entryNode": {
                  "type": "string",
                  "examples": ["ma1"]
                },
                "optim": {
                  "type": "object",
                  "properties": {
                    "algorithm": {
                      "type": "string",
                      "default": "Nelder-Mead",
                      "examples": ["Nelder-Mead"]
                    }
                  }
                },
                "operators": {
                  "type": "array",
                  "items": {
                    "type": "object",
                    "properties": {
                      "id": {
                        "type": "string",
                        "examples": ["ma1"]
                      },
                      "type": {
                        "type": "string",
                        "examples": ["MA"]
                      },
                      "parameters": {
                        "type": "array",
                        "items": {
                          "type": "object",
                          "properties": {
                            "name": {
                              "type": "string",
                              "examples": ["backwardTimeWindow"]
                            },
                            "type": {
                              "enum": ["integer", "float"]
                            },
                            "value": {
                              "type": "number",
                              "examples": [2, 0.33]
                            },
                            "optim": {
                              "type": "object",
                              "properties": {
                                "range": {
                                  "type": "object",
                                  "properties": {
                                    "min": {
                                      "type": "number",
                                      "examples": [0]
                                    },
                                    "max": {
                                      "type": "number",
                                      "examples": [10]
                                    }
                                  },
                                  "additionalProperties": false
                                }
                              },
                              "additionalProperties": false
                            }
                          },
                          "required": ["name", "type", "value"],
                          "additionalProperties": false
                        }
                      }
                    },
                    "required": ["id", "type"],
                    "additionalProperties": true
                  }
                },
                "connections": {
                  "type": "array",
                  "items": {
                    "type": "object",
                    "properties": {
                      "from": {
                        "type": "string",
                        "examples": ["ma1"]
                      },
                      "to": {
                        "type": "string",
                        "examples": ["std"]
                      }
                    },
                    "required": ["from", "to"],
                    "additionalProperties": false
                  }
                }
              },
              "required": ["title", "entryNode", "operators", "connections"],
              "additionalProperties": true
            }),
        }
    }

    pub fn validate(&self, program: &Value) -> Result<(), String> {
        let compiled = JSONSchema::compile(&self.schema)
            .map_err(|err| format!("Invalid schema:\n{:?}", err))?;
        compiled.validate(program).map_err(|err| {
            let mut msg = "".to_string();
            for e in err {
                msg.push_str(format!("Validation error: {}\n", e).as_str());
                msg.push_str(format!("path: {}\n", e.instance_path).as_str());
            }
            msg
        })
    }
}
