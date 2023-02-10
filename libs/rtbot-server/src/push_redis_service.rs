extern crate redis;

use futures_util::StreamExt as _;
use redis::cmd;
use std::sync::{Arc, Mutex};

struct RedisProgramKeys {
    program_key: String,
    input_key: String,
    output_key: String,
    output_pubsub_key: String,
}

pub struct PushRedisService {
    url: String,
    program: String,
    keys: Option<RedisProgramKeys>,
    connection: Option<redis::aio::Connection>,
}

impl PushRedisService {
    pub fn new(url: String, program: String) -> Self {
        Self {
            url,
            program,
            connection: None,
            keys: None,
        }
    }

    pub async fn start(&mut self) -> redis::RedisResult<String> {
        info!("Connecting to redis {}", self.url.as_str());
        // connect
        let client = redis::Client::open(self.url.as_str())?;
        let mut con = client.get_async_connection().await?;
        // write the program to redis
        let program_id = nanoid::nanoid!(5, &nanoid::alphabet::SAFE);
        let program_key = format!("p:{}", program_id);
        let input_key = format!("p:i:{}", program_id);
        let output_key = format!("p:o:{}", program_id);
        let output_pubsub_key = format!("p:o:{}:ps", program_id);
        info!(
            "Redis program, input and output keys: {}, {}, {}",
            program_key, input_key, output_key
        );
        // create the input timeseries key
        cmd("TS.CREATE")
            .arg(&input_key)
            .arg("DUPLICATE_POLICY")
            .arg("LAST")
            .query_async::<_, ()>(&mut con)
            .await?;
        // store the program inside redis
        cmd("JSON.SET")
            .arg(&program_key)
            .arg("$")
            .arg(&self.program)
            .query_async::<_, ()>(&mut con)
            .await?;
        // run rtbot.xrun command
        cmd("RTBOT.XRUN")
            .arg(&program_key)
            .arg(&input_key)
            .arg(&output_key)
            .query_async::<_, ()>(&mut con)
            .await?;

        // store the objects
        self.connection = Some(con);
        self.keys = Some(RedisProgramKeys {
            program_key,
            input_key,
            output_key,
            output_pubsub_key: output_pubsub_key.to_string(),
        });
        Ok(output_pubsub_key)
    }

    pub async fn add(&mut self, timestamp: u64, values: Vec<f64>) -> redis::RedisResult<()> {
        let mut con = self.connection.as_mut().unwrap();
        let keys = self.keys.as_ref().unwrap();

        cmd("TS.ADD")
            .arg(keys.input_key.as_str())
            .arg(timestamp)
            .arg(values[0])
            .query_async::<_, ()>(con)
            .await?;
        Ok(())
    }
}
