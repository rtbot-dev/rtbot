use std::collections::BTreeMap;
use std::sync::{Arc, Mutex, RwLock};
use redis_module::{RedisError, RedisResult, RedisValue};

use redis_module::RedisError::Str;
use crate::cxx_bindings;
use crate::cxx_bindings::ffi::RtBotMessage;

pub static PIPELINES_REGISTRY: RwLock<Option<PipelinesRegistry>> = RwLock::new(None);

pub struct PipelinesRegistry {
    pipelines:Arc<Mutex<BTreeMap<String, BTreeMap<String, String>>>>
}

impl PipelinesRegistry {
    pub fn new() -> Self {
        Self {
            pipelines: Arc::new(Mutex::new(BTreeMap::new()))
        }
    }

    /// Creates a pipeline that connects the data at the input key
    /// to the value at the output key. Once the pipeline between
    /// these two is created we can send data to it and the result
    /// can be used to update the value existing at the output key.
    /// Notice that the last is not done here, here we just focus on
    /// keeping track of which keys are connected with which, and to
    /// keep the connection among those, a pipeline, in memory such
    /// that the pipeline state is not lost.
    ///
    /// # Arguments
    ///
    /// * `program_json_str`: The programs which defines the pipeline.
    /// * `input_key`: The input key.
    /// * `output_key`: The output key.
    ///
    /// returns: Result<String, RedisError>
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
        println!("Sending program {}", program_json_str);
        let result = unsafe { cxx_bindings::ffi::create_pipeline(id.to_string(), program_json_str.to_string()) };
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

    /// Deletes the pipeline registered between the input and output keys specified,
    /// if any.
    ///
    /// # Arguments
    ///
    /// * `input_key`: The input key.
    /// * `output_key`: The output key.
    ///
    /// returns: Result<String, RedisError>
    pub fn delete(&self, input_key: &String, output_key: &String) -> Result<String, RedisError> {
        let mut pipeline = self.pipelines.lock().unwrap();
        if let Some(outputs) = pipeline.get_mut(input_key) {
            if let Some(pipeline_id) = outputs.remove(output_key) {
                let result = unsafe { cxx_bindings::ffi::delete_pipeline(pipeline_id.to_string()) };
                return if result != "" {
                    Err(RedisError::String(format!("Unable to delete pipeline {}: {}", pipeline_id, result)))
                } else {
                    Ok("OK".into())
                }
            }
        }

        Err(RedisError::String(format!("There is no pipeline from key {}", input_key.to_string())))
    }

    /// Delete all the pipelines attached to a given input key.
    /// Internally it will look for all existing pipelines that
    /// starts at the given input key and ends at an existing
    /// output key and will invoke `delete` on each of these.
    ///
    /// # Arguments
    ///
    /// * `input_key`: The input key.
    ///
    /// returns: Result<String, RedisError>
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

    /// Sends a message to all the pipelines attached to the given `input_key`
    /// Returns a map where the keys are the output keys that are bound to the
    /// input key with an existing pipeline, and the value is the result of
    /// sending the given message to the existing pipeline. If the returned vector
    /// is empty it means that there was no output for the current rtbot iteration.
    ///
    /// # Arguments
    ///
    /// * `input_key`: The input key
    /// * `timestamp`: The timestamp of the message
    /// * `values`: The values of the message.
    ///
    /// returns: Result<BTreeMap<String, Vec<RtBotMessage, Global>, Global>, RedisError>
    pub fn receive(&self, input_key: &String, timestamp: u64, values: Vec<f64>) -> Result<BTreeMap<String, Vec<RtBotMessage>>, RedisError> {
        let pipeline = self.pipelines.lock().unwrap();
        let mut result = BTreeMap::new();
        if let Some(outputs) = pipeline.get(input_key) {
            for (output_key, pipeline_id) in outputs {
                let message = RtBotMessage {
                    timestamp,
                    values: values.clone()
                };
                let r = unsafe { cxx_bindings::ffi::receive_message_in_pipeline(pipeline_id.to_string(), message) };
                result.insert(output_key.to_string(), r);
            }
        }
        Ok(result)
    }
}