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

pub struct PullRedisService {
    url: String,
    output_pubsub_key: String,
}

impl PullRedisService {
    pub fn new(url: String, output_pubsub_key: String) -> Self {
        Self {
            url,
            output_pubsub_key,
        }
    }

    pub async fn subscribe_to_redis_rtbot_messages(&self) -> redis::RedisResult<()> {
        info!("Subscribing to pubsub channel {}", self.output_pubsub_key);
        let client = redis::Client::open(self.url.as_str()).unwrap();
        let mut pubsub_conn = client.get_async_connection().await?.into_pubsub();
        pubsub_conn
            .subscribe(self.output_pubsub_key.as_str())
            .await?;
        let mut pubsub_stream = pubsub_conn.on_message();

        while let Some(msg) = pubsub_stream.next().await {
            let pubsub_msg: String = msg.get_payload()?;
            let arr = pubsub_msg.split(",");
            let mut it = arr.into_iter();
            let timestamp: u64 = it.next().unwrap().parse().unwrap();
            let mut values = vec![];
            for v in it {
                values.push(v);
            }

            info!("Received message parsed: ({}, {:?}", timestamp, values);
        }

        Ok(())
    }
}
