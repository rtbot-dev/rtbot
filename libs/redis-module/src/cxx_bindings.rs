#[cxx::bridge(namespace = "rtbot")]
pub mod ffi {
    pub struct RtBotMessage {
        timestamp: u64,
        values: Vec<f64>
    }

    unsafe extern "C++" {
        include!("bindings.h");

        pub fn createPipeline(id: String, program: String) -> String;
        pub fn deletePipeline(id: String) -> String;
        pub fn receiveMessageInPipeline(id:String, timestamp: u64, values: &[f64]) -> Vec<RtBotMessage>;
    }
}