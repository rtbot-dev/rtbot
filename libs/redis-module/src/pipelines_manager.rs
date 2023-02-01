use std::collections::BTreeMap;
use std::ffi::CString;
use std::sync::RwLock;
use redis_module::{RedisError, RedisResult};

use redis_module::RedisError::Str;

pub static PIPELINES_MANAGER: RwLock<Option<PipelinesManager>> = RwLock::new(None);

pub struct PipelinesManager {
    pipelines: BTreeMap<String, BTreeMap<String, String>>
}

impl PipelinesManager {
    pub fn new() -> Self {
        Self {
            pipelines: BTreeMap::new()
        }
    }

    pub fn create(&mut self, program_json_str: String, input_key: String, output_key: String) ->  Result<String, RedisError> {
        if self.pipelines.contains_key(&input_key) {
            if let Some(outputs) = self.pipelines.get(&input_key) {
                if let Some(pipeline_id) = outputs.get(&output_key) {
                    return Err(RedisError::String(format!("There is already a pipeline, id {}, running for input {} to output {}", pipeline_id, input_key, output_key)));
                }
            }
        }
        let id = nanoid::nanoid!(5);
        let result = unsafe { rtbot_bindgen::createPipeline(CString::new(id.to_string())?.as_c_str().as_ptr(), CString::new(program_json_str)?.as_c_str().as_ptr()) };
        if result != 0 {
            Err(Str("Unable to create pipeline from program"))
        } else {
            if self.pipelines.get(&input_key).is_none() {
                self.pipelines.insert(input_key.to_string(), BTreeMap::new());
            }

            self.pipelines.get_mut(&input_key).unwrap().insert(output_key, id.to_string());
            Ok(id)
        }
    }

    pub fn delete(&mut self, id: String) -> Result<String, RedisError> {
        let result = unsafe { rtbot_bindgen::deletePipeline(CString::new(id.to_string())?.as_c_str().as_ptr()) };
        if result != 0 {
            Err(RedisError::String(format!("Unable to delete pipeline {}", id.to_string())))
        } else {
            self.pipelines.remove(&id);
            Ok(id.to_string())
        }
    }
}