use crate::Rx;
use futures_util::{SinkExt, StreamExt};
use serde::Serialize;
use serde_json::json;
use std::convert::Infallible;
use thiserror::Error;
use warp::filters::ws::WebSocket;
use warp::http::StatusCode;
use warp::ws::Message;

#[derive(Serialize, Debug)]
struct ApiErrorResult {
    detail: String,
}

// errors thrown by handlers and custom filters,
// such as `ensure_authentication` filter
#[derive(Error, Debug)]
enum ApiErrors {
    #[error("user not authorized")]
    NotAuthorized(String),
}

pub async fn handle_rejection(
    err: warp::reject::Rejection,
) -> Result<impl warp::reply::Reply, Infallible> {
    let code;
    let message;

    if err.is_not_found() {
        code = StatusCode::NOT_FOUND;
        message = "Not found";
    } else if let Some(_) = err.find::<warp::filters::body::BodyDeserializeError>() {
        code = StatusCode::BAD_REQUEST;
        message = "Invalid Body";
    } else if let Some(e) = err.find::<ApiErrors>() {
        match e {
            ApiErrors::NotAuthorized(_error_message) => {
                code = StatusCode::UNAUTHORIZED;
                message = "Action not authorized";
            }
        }
    } else if let Some(_) = err.find::<warp::reject::MethodNotAllowed>() {
        code = StatusCode::METHOD_NOT_ALLOWED;
        message = "Method not allowed";
    } else {
        // We should have expected this... Just log and say its a 500
        error!("unhandled rejection: {:?}", err);
        code = StatusCode::INTERNAL_SERVER_ERROR;
        message = "Internal server error";
    }

    let json = warp::reply::json(&ApiErrorResult {
        detail: message.into(),
    });

    Ok(warp::reply::with_status(json, code))
}

pub async fn handle_ws_client(websocket: WebSocket, rx: Rx) {
    // receiver - this server, from websocket client
    // sender - diff clients connected to this server
    let (mut client_ws_sender, _) = websocket.split();

    let mut receiver = rx.write().await;
    while let Some(msg) = receiver.recv().await {
        // TODO: spawn a tokio task here to unblock the thread
        let mut it = msg.iter();
        let timestamp = it.next().unwrap().parse::<u64>().unwrap();
        let mut values = vec![];
        for v in it {
            values.push(v);
        }

        let payload = json!({
            "timestamp": timestamp,
            "values": values
        })
        .to_string();

        debug!("Sending data to websocket {}", payload);
        client_ws_sender
            .send(Message::text(payload))
            .await
            .expect("Unable to send message to socket");
    }

    info!("client disconnected");
}
