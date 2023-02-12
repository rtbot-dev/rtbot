extern crate redis;

use redis::cmd;

pub struct PushRedisService {
    url: String,
    program: String,
    input_key: Option<String>,
    connection: Option<redis::aio::Connection>,
}

impl PushRedisService {
    pub fn new(url: String, program: String) -> Self {
        Self {
            url,
            program,
            connection: None,
            input_key: None,
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
        self.input_key = Some(input_key);
        Ok(output_pubsub_key)
    }

    pub async fn add(&mut self, timestamp: u64, values: Vec<f64>) -> redis::RedisResult<()> {
        let con = self.connection.as_mut().unwrap();
        let input_key = self.input_key.as_ref().unwrap();

        cmd("TS.ADD")
            .arg(input_key.as_str())
            .arg(timestamp)
            .arg(values[0])
            .query_async::<_, ()>(con)
            .await?;
        Ok(())
    }
}
