#[cxx::bridge(namespace = "rtbot")]
pub mod ffi {
    #[derive(Debug)]
    pub struct RtBotMessage {
        pub(crate) timestamp: u64,
        pub(crate) values: Vec<f64>
    }

    unsafe extern "C++" {
        include!("bindings.h");

        pub fn createPipeline(id: String, program: String) -> String;
        pub fn deletePipeline(id: String) -> String;
        pub fn receiveMessageInPipeline(id:String, message: RtBotMessage) -> Vec<RtBotMessage>;
    }
}