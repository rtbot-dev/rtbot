#[cxx::bridge(namespace = "rtbot")]
pub mod ffi {
    #[derive(Debug)]
    pub struct RtBotMessage {
        pub(crate) timestamp: u64,
        pub(crate) values: Vec<f64>
    }

    unsafe extern "C++" {
        include!("cxx_bindings.h");

        pub fn create_pipeline(id: String, program: String) -> String;
        pub fn delete_pipeline(id: String) -> String;
        pub fn receive_message_in_pipeline(id:String, message: RtBotMessage) -> Vec<RtBotMessage>;
    }
}