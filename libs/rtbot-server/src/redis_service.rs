extern crate redis;

use redis::cmd;
use std::sync::{Arc, Mutex};

pub struct RedisService {
    url: String,
    program: String,
    program_key: Option<String>,
    input_key: Option<String>,
    output_key: Option<String>,
    connection: Arc<Mutex<Option<redis::aio::Connection>>>,
}

impl RedisService {
    pub fn new(url: String, program: String) -> Self {
        Self {
            url,
            program,
            connection: Arc::new(Mutex::new(None)),
            program_key: None,
            input_key: None,
            output_key: None,
        }
    }

    pub async fn start(&mut self) -> redis::RedisResult<()> {
        // connect
        let client = redis::Client::open(self.url.as_str())?;
        let mut con = client.get_async_connection().await?;
        // write the program to redis
        let program_key = format!("p:{}", nanoid::nanoid!(5));
        let input_key = format!("{}:i", program_key);
        let output_key = format!("{}:o", program_key);
        info!(
            "Redis program, input and output keys: {}, {}, {}",
            program_key, input_key, output_key
        );
        // create the input timeseries key
        cmd("TS.CREATE")
            .arg(&input_key)
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
        self.connection = Arc::new(Mutex::new(Some(con)));
        self.program_key = Some(program_key);
        self.input_key = Some(input_key);
        self.output_key = Some(output_key);
        Ok(())
    }

    pub async fn add(&self, timestamp: u64, values: Vec<f64>) -> redis::RedisResult<()> {
        let mut con_guard = self.connection.lock().unwrap();
        let con = con_guard.as_mut().unwrap();

        cmd("TS.ADD")
            .arg(&self.input_key)
            .arg(timestamp)
            .arg(values[0])
            .query_async::<_, ()>(con)
            .await?;
        Ok(())
    }

    pub async fn subscribe(&self) {
        todo!()
    }
}
