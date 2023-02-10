extern crate jsonpath_rust;
extern crate pretty_env_logger;
extern crate serde_json;

use crate::config::{InputWsConfig, ServerConfig};
use crate::pull_redis_service::PullRedisService;
use crate::push_redis_service::PushRedisService;
use futures_util::StreamExt;
use jsonpath_rust::JsonPathFinder;
use serde_json::Value;
use std::sync::{Arc, Mutex};
use std::time::{SystemTime, UNIX_EPOCH};
use tokio_tungstenite::tungstenite::Message::Text;
use tokio_tungstenite::{connect_async, tungstenite::Result};
use url::Url;

#[macro_use]
extern crate log;

mod config;
mod pull_redis_service;
mod push_redis_service;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    pretty_env_logger::init();
    let config = ServerConfig::new().expect("Unable to find a configuration file");
    info!("Server config {:#?}", config);
    // TODO: handle json and yaml cases for program configuration
    let program = config.rtbot.program.json.unwrap();

    if let Some(mut redis_config) = config.redis {
        info!("Initializing redis");
        let mut push_redis_service = PushRedisService::new(redis_config.url.to_string(), program);
        let output_pubsub_key = push_redis_service
            .start()
            .await
            .expect("Unable to connect to redis");

        if let Some(input_ws) = config.input_ws {
            // subscribe to output messages coming from redis
            // we will create a second
            let url = redis_config.url.to_string();
            tokio::spawn(async move {
                let pull_redis_service = PullRedisService::new(url, output_pubsub_key);
                pull_redis_service.subscribe_to_redis_rtbot_messages().await;
            });
            // connect to the input websocket and stream data to redis
            connect_to_input_ws_redis(&input_ws, &mut push_redis_service).await;
        }
    }

    Ok(())
}

async fn connect_to_input_ws_redis(input_ws: &InputWsConfig, redis_service: &mut PushRedisService) {
    let case_url = Url::parse(input_ws.url.as_str()).expect("Bad websocket connection url");

    let (mut ws_input_stream, _) = connect_async(case_url).await.unwrap();

    while let Some(msg) = ws_input_stream.next().await {
        let msg = msg.unwrap();
        if let Text(payload) = msg {
            trace!("Message {:#?}", payload);
            // compute the timestamp
            let timestamp: u64 = if let Some(mapping) = &input_ws.json_remap.timestamp {
                let timestamp_finder = JsonPathFinder::from_str(&payload, mapping).unwrap().find();
                timestamp_finder
                    .as_array()
                    .unwrap()
                    .first()
                    .unwrap()
                    .as_u64()
                    .unwrap()
            } else {
                SystemTime::now()
                    .duration_since(UNIX_EPOCH)
                    .unwrap()
                    .as_millis()
                    .try_into()
                    .expect("Unable to cast timestamp value from 'now()' to u64")
            };

            // compute the values
            let values: Vec<Option<f64>> = input_ws
                .json_remap
                .values
                .iter()
                .map(|mapping| {
                    let value_finder = JsonPathFinder::from_str(&payload, mapping).unwrap().find();
                    if let Value::String(value_str) =
                        value_finder.as_array().unwrap().first().unwrap()
                    {
                        match value_str.parse::<f64>() {
                            Ok(v) => Some(v),
                            Err(_) => {
                                warn!(
                                    "Unable to parse float from message {:#?}, mapping {}",
                                    payload, mapping
                                );
                                None
                            }
                        }
                    } else {
                        None
                    }
                })
                .collect();

            info!("timestamp {:?}, values {:?}", timestamp, values);

            // Notice we are sending here only the first parsed value
            if let Err(e) = redis_service.add(timestamp, vec![values[0].unwrap()]).await {
                warn!("Unable to store the data in redis: {:#?}", e);
            };
        }
    }
}
