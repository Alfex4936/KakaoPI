#![feature(proc_macro_hygiene, decl_macro)]
extern crate serde_derive;
extern crate serde_json;

// 파이2에서 실행되는 IP
pub const SERVER: &str = "0.0.0.0:8010";
pub const TCP_SERVER: &str = env!("PI_TCP_SERVER");

// Event API를 사용하면 메시지를 먼저 보낼 수 있지만 개당 15원
mod routes;
pub use routes::sensor;
pub use routes::util;
