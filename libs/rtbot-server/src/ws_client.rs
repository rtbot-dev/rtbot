use futures_util::stream::SplitSink;
use futures_util::SinkExt;
use serde_json::json;
use tokio::sync::mpsc;
use tokio::sync::mpsc::error::SendError;
use tokio::sync::mpsc::UnboundedReceiver;
use warp::ws::{Message, WebSocket};
use warp::Error;

#[derive(Debug)]
pub enum ClientMessage {
    Data { timestamp: u64, values: Vec<String> },
}

struct Client {
    receiver: UnboundedReceiver<ClientMessage>,
    ws_sender: SplitSink<WebSocket, Message>,
}

impl Client {
    fn new(
        receiver: UnboundedReceiver<ClientMessage>,
        ws_sender: SplitSink<WebSocket, Message>,
    ) -> Self {
        Client {
            receiver,
            ws_sender,
        }
    }

    pub async fn handle_message(&mut self, msg: ClientMessage) {
        debug!("Receiving message {:?} on client", msg);
        if let ClientMessage::Data { timestamp, values } = msg {
            self.ws_sender
                .send(Message::text(
                    json!({
                        "timestamp": timestamp,
                        "values": values
                    })
                    .to_string(),
                ))
                .await
                .expect("Unable to send ws message, disconnecting client");
        }
    }
}

#[derive(Clone)]
pub struct ClientHandle {
    sender: mpsc::UnboundedSender<ClientMessage>,
}

impl ClientHandle {
    pub fn new(ws_sender: SplitSink<WebSocket, Message>) -> Self {
        let (sender, receiver) = mpsc::unbounded_channel();
        let client = Client::new(receiver, ws_sender);
        tokio::spawn(run_client(client));

        Self { sender }
    }

    pub fn send(&self, msg: ClientMessage) -> Result<(), SendError<ClientMessage>> {
        self.sender.send(msg)
    }
}

async fn run_client(mut client: Client) {
    while let Some(msg) = client.receiver.recv().await {
        client.handle_message(msg).await;
    }
}
