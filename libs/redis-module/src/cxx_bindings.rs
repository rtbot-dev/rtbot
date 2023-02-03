#[cxx::bridge(namespace = "rtbot")]
pub mod ffi {
    pub struct RtBotMessage {
        timestamp: u64,
        values: Vec<f64>
    }

    unsafe extern "C++" {
        include!("bindings.h");

        pub fn createPipeline<'a>(id: &'a str, program: &'a str) -> &'a str;
        pub fn deletePipeline(id: &str) -> &str;
        pub fn receiveMessageInPipeline(id:&str, timestamp: u64, values: &[f64]) -> Vec<RtBotMessage>;
    }
}