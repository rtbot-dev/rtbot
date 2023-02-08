use config::{Config, ConfigError, File};
use glob::glob;
use serde::Deserialize;

#[derive(Debug, Deserialize)]
#[allow(unused)]
pub struct ServerConfig {
    pub url: String,
    pub rtbot: RtBotConfig,
    pub redis: Option<RedisConfig>,
    pub input_ws: Option<InputWsConfig>,
}

#[derive(Debug, Deserialize)]
#[allow(unused)]
pub struct InputWsConfig {
    pub url: String,
    pub operator_id: String,
    pub json_remap: InputJsonPath,
}

/// @see https://goessner.net/articles/JsonPath/
#[derive(Debug, Deserialize)]
#[serde(default)]
#[allow(unused)]
pub struct InputJsonPath {
    // this field is optional as if in the case it hasn't been set we
    // use instead the server arrival time as timestamp
    pub timestamp: Option<String>,
    pub values: Vec<String>,
}

impl Default for InputJsonPath {
    fn default() -> Self {
        Self {
            timestamp: Some("$.timestamp".to_string()),
            values: vec!["$.value".to_string()],
        }
    }
}

#[derive(Debug, Deserialize)]
#[allow(unused)]
pub struct RedisConfig {
    pub connection: String,
    pub input_key: String,
    pub output_key: String,
}

#[derive(Debug, Deserialize)]
#[allow(unused)]
pub struct RtBotConfig {
    pub program: RtBotConfigProgram,
}

#[derive(Debug, Deserialize)]
#[allow(unused)]
pub struct RtBotConfigProgram {
    pub json: Option<String>,
    pub yaml: Option<String>,
}

impl ServerConfig {
    pub fn new() -> Result<Self, ConfigError> {
        let s = Config::builder()
            .add_source(
                glob("libs/rtbot-server/resources/*")
                    .unwrap()
                    .map(|path| File::from(path.unwrap()))
                    .collect::<Vec<_>>(),
            )
            .add_source(
                glob("resources/*")
                    .unwrap()
                    .map(|path| File::from(path.unwrap()))
                    .collect::<Vec<_>>(),
            )
            .build()?;

        // You can deserialize (and thus freeze) the entire configuration as
        s.try_deserialize::<ServerConfig>()
    }
}
