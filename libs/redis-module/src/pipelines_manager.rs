use std::collections::BTreeMap;
use std::sync::{Arc, Mutex, RwLock};
use redis_module::RedisError;

use redis_module::RedisError::Str;
use crate::cxx_bindings;

pub static PIPELINES_MANAGER: RwLock<Option<PipelinesManager>> = RwLock::new(None);

pub struct PipelinesManager {
    pipelines:Arc<Mutex<BTreeMap<String, BTreeMap<String, String>>>>
}

impl PipelinesManager {
    pub fn new() -> Self {
        Self {
            pipelines: Arc::new(Mutex::new(BTreeMap::new()))
        }
    }

    pub fn create(&self, program_json_str: &String, input_key: &String, output_key: &String) ->  Result<String, RedisError> {
        let mut pipeline = self.pipelines.lock().unwrap();
        if pipeline.contains_key(input_key) {
            if let Some(outputs) = pipeline.get(input_key) {
                if let Some(pipeline_id) = outputs.get(output_key) {
                    return Err(RedisError::String(format!("There is already a pipeline, id {}, running for input {} to output {}", pipeline_id, input_key, output_key)));
                }
            }
        }
        let id = nanoid::nanoid!(5);
        let result = unsafe { cxx_bindings::ffi::createPipeline(&id, program_json_str) };
        if result != "" {
            Err(RedisError::String(format!("Unable to create pipeline from program: {}", result)))
        } else {
            if pipeline.get(input_key).is_none() {
                pipeline.insert(input_key.to_string(), BTreeMap::new());
            }

            let mut outputs = pipeline.get_mut(input_key).unwrap();
            outputs.insert(output_key.to_string(), id.to_string());

            Ok(id.into())
        }
    }

    pub fn delete(&self, input_key: &String, output_key: &String) -> Result<String, RedisError> {
        let mut pipeline = self.pipelines.lock().unwrap();
        if let Some(outputs) = pipeline.get_mut(input_key) {
            if let Some(pipeline_id) = outputs.remove(output_key) {
                let result = unsafe { cxx_bindings::ffi::deletePipeline(&pipeline_id) };
                return if result != "" {
                    Err(RedisError::String(format!("Unable to delete pipeline {}: {}", &pipeline_id, result)))
                } else {
                    Ok("OK".into())
                }
            }
        }

        Err(RedisError::String(format!("There is no pipeline from key {}", input_key.to_string())))
    }

    pub fn delete_by_input_key(&self, input_key: &String) -> Result<String, RedisError> {
        let pipeline = self.pipelines.lock().unwrap();
        let mut counter = 0;
        if let Some(outputs) = pipeline.get(input_key) {
            for (output_key, _) in outputs {
                self.delete(input_key, output_key)?;
                counter += 1;
            }
        }
        Ok(format!("Deleted {} pipeline(s) associated with the input key {}", counter, input_key).into())
    }

    pub fn receive(&self, input_key: &String, timestamp: u64, values: Vec<f64>) -> Result<String, RedisError> {
        let pipeline = self.pipelines.lock().unwrap();
        let values_slice = values.as_slice();
        Ok("".into())
    }
}