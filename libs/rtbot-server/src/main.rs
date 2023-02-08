extern crate jsonpath_rust;
extern crate pretty_env_logger;
extern crate serde_json;

use crate::config::{InputWsConfig, ServerConfig};
use futures_util::StreamExt;
use jsonpath_rust::JsonPathFinder;
use serde_json::Value;
use std::num::ParseFloatError;
use std::time::{SystemTime, UNIX_EPOCH};
use tokio_tungstenite::tungstenite::Message::Text;
use tokio_tungstenite::{
    connect_async,
    tungstenite::{Error, Message, Result},
};
use url::Url;

#[macro_use]
extern crate log;

mod config;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    pretty_env_logger::init();
    let config = ServerConfig::new().expect("Unable to find a configuration file");
    info!("Server config {:#?}", config);
    if let Some(input_ws) = config.input_ws {
        connect_to_input_ws(&input_ws).await;
    }
    Ok(())
}

async fn connect_to_input_ws(input_ws: &InputWsConfig) {
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
        }
    }
}
