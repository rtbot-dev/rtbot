extern crate redis;

use futures_util::StreamExt as _;
use tokio::sync::mpsc::UnboundedSender;

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

    pub async fn subscribe_to_redis_rtbot_messages(
        &self,
        tx: UnboundedSender<Vec<String>>,
    ) -> redis::RedisResult<()> {
        info!("Subscribing to pubsub channel {}", self.output_pubsub_key);
        let client = redis::Client::open(self.url.as_str()).unwrap();
        let mut pubsub_conn = client.get_async_connection().await?.into_pubsub();
        pubsub_conn
            .subscribe(self.output_pubsub_key.as_str())
            .await?;
        let mut pubsub_stream = pubsub_conn.on_message();

        while let Some(msg) = pubsub_stream.next().await {
            let pubsub_msg: String = msg.get_payload()?;
            // notify that we have received data to other parts of the program
            let tx_msg = pubsub_msg
                .split(",")
                .map(|s| s.to_string())
                .collect::<Vec<_>>();
            tx.send(tx_msg)
                .expect("Unable to notify the arrival of new data from redis");
        }

        Ok(())
    }
}
