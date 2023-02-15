extern crate jsonpath_rust;
extern crate pretty_env_logger;
extern crate serde_json;

use crate::config::{InputWsConfig, ServerConfig};
use crate::pull_redis_service::PullRedisService;
use crate::push_redis_service::PushRedisService;
use crate::ws_handlers::{handle_rejection, handle_ws_client};
use futures_util::StreamExt;
use jsonpath_rust::JsonPathFinder;
use serde_json::Value;
use std::collections::HashMap;

use crate::ws_client::{ClientHandle, ClientMessage};
use std::net::{IpAddr, Ipv4Addr, SocketAddr};
use std::ops::Deref;
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};
use tokio::sync::mpsc::error::SendError;
use tokio::sync::mpsc::UnboundedReceiver;
use tokio::sync::RwLock;
use tokio_tungstenite::tungstenite::Message::Text;
use tokio_tungstenite::{connect_async, tungstenite::Result};
use url::Url;
use warp::Filter;

#[macro_use]
extern crate log;

mod config;
mod pull_redis_service;
mod push_redis_service;
mod ws_client;
mod ws_handlers;

type Rx = Arc<RwLock<UnboundedReceiver<Vec<String>>>>;
type Clients = Arc<RwLock<HashMap<String, ClientHandle>>>;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    pretty_env_logger::init();
    let config = ServerConfig::new().expect("Unable to find a configuration file");
    debug!("Server config {:#?}", config);
    // TODO: handle json and yaml cases for program configuration
    let program = config.rtbot.program.json.unwrap();
    let (tx, mut rx) = tokio::sync::mpsc::unbounded_channel::<Vec<String>>();
    let clients: Clients = Arc::new(RwLock::new(HashMap::new()));
    let clients_container = clients.clone();

    // send the message to each registered client
    // every time we receive a message from redis
    tokio::spawn(async move {
        while let Some(msg) = rx.recv().await {
            let mut clients = clients.write().await;
            let mut disconnected_clients: Vec<String> = vec![];
            for (id, client_handle) in clients.iter() {
                // send the message to all client handles
                let ref_values = &msg[1..];
                let values = ref_values
                    .iter()
                    .map(|s| s.to_string())
                    .collect::<Vec<String>>();

                if let Err(SendError(_)) = client_handle.send(ClientMessage::Data {
                    timestamp: msg[0].parse().unwrap(),
                    values,
                }) {
                    info!("Disconnecting client {}", id);
                    disconnected_clients.push(id.to_string());
                }
            }

            // remove all the clients disconnected from the internal list
            for disconnected_client in disconnected_clients {
                clients.remove(disconnected_client.as_str());
            }
        }
    });

    if let Some(redis_config) = config.redis {
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
                pull_redis_service
                    .subscribe_to_redis_rtbot_messages(tx)
                    .await
                    .expect("Unable to read pubsub data coming from redis");
            });

            // connect to the input websocket and stream data to redis
            tokio::spawn(async move {
                connect_to_input_ws_redis(&input_ws, &mut push_redis_service).await;
            });
        }
    }

    // websocket server code
    let health_check = warp::path("health-check").map(|| format!("Server OK"));

    let ws = warp::path("ws")
        .and(warp::ws())
        .and(warp::any().map(move || clients_container.clone()))
        .map(|ws: warp::ws::Ws, clients: Clients| {
            info!("upgrading connection to websocket");
            ws.on_upgrade(move |ws| handle_ws_client(ws, clients))
        });

    let routes = health_check
        .or(ws)
        .with(warp::cors().allow_any_origin())
        .recover(handle_rejection);

    warp::serve(routes)
        .run(SocketAddr::new(
            IpAddr::V4(Ipv4Addr::new(127, 0, 0, 1)),
            config.port.unwrap_or(9889),
        ))
        .await;
    info!("server is running");

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

            debug!("timestamp {:?}, values {:?}", timestamp, values);

            // Notice we are sending here only the first parsed value
            if let Err(e) = redis_service.add(timestamp, vec![values[0].unwrap()]).await {
                warn!("Unable to store the data in redis: {:#?}", e);
            };
        }
    }
}
