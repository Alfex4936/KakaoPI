#![allow(proc_macro_derive_resolution_fallback)]
use actix_web::{post, web, HttpResponse, Responder};
use async_std::net::TcpStream;
use async_std::prelude::*;
use kakao_rs::prelude::*;
use serde_json::Value;
use std::sync::Mutex;
use subprocess::{Exec, Popen, PopenConfig, Redirection};

#[post("/v1/util/gpio")] // linux -> gpio readall로 대체 가능
async fn gpio_readall() -> impl Responder {
    println!("=> gpio readall");
    let mut result = Template::new();

    result.add_qr(QuickReply::new("gpio readall", "gpio readall"));

    let mut proc = Popen::create(
        &["gpio", "readall"],
        PopenConfig {
            stdout: Redirection::Pipe,
            stdin: Redirection::None,
            stderr: Redirection::None,
            ..Default::default()
        },
    )
    .unwrap();
    // Obtain the output from the standard streams.
    let (out, _) = proc.communicate(None).unwrap();
    let out = out.unwrap();

    result.add_output(SimpleText::new(out).build());

    HttpResponse::Ok()
        .content_type("aplication/json")
        .body(serde_json::to_string(&result).unwrap())
}

#[post("/v1/util/linux")] // pipe 없는 일반 커맨드만 일단 지원, 예전 운영체제 과제처럼 하면 될 듯 하긴함
async fn run_linux_cmd(kakao: web::Json<Value>) -> impl Responder {
    println!("=> 파이1 linux 명령어 실행");
    let mut result = Template::new();

    let kakao_action = &kakao["action"]["params"];
    let action = match kakao_action.get("cmd") {
        Some(v) => v.as_str().unwrap(),
        _ => {
            result.add_qr(QuickReply::new("linux", "명령어 실행"));
            result.add_output(SimpleText::new("명령어를 확인해주세요.").build());

            return HttpResponse::Ok()
                .content_type("application/json")
                .body(serde_json::to_string(&result).unwrap());
        }
    };

    let cmd: Vec<&str> = action.split(" ").map(|s| s).collect();
    if cmd[0].eq("sudo") {
        result.add_qr(QuickReply::new("linux", "명령어 실행"));
        result.add_output(SimpleText::new("sudo 금지").build());

        return HttpResponse::Ok()
            .content_type("application/json")
            .body(serde_json::to_string(&result).unwrap());
    }

    result.add_qr(QuickReply::new("linux", "linux"));

    let proc = Popen::create(
        &cmd,
        PopenConfig {
            stdout: Redirection::Pipe,
            stdin: Redirection::None,
            stderr: Redirection::Merge,
            ..Default::default()
        },
    );

    let mut proc = match proc {
        Ok(proc) => proc,
        Err(_) => {
            result.add_qr(QuickReply::new("linux", "명령어 실행"));
            result.add_output(SimpleText::new("존재하지 않는 명령어입니다.").build());

            return HttpResponse::Ok()
                .content_type("application/json")
                .body(serde_json::to_string(&result).unwrap());
        }
    };

    let (out, _) = proc.communicate(None).unwrap();
    let out = out.unwrap();

    result.add_output(SimpleText::new(out).build());

    HttpResponse::Ok()
        .content_type("aplication/json")
        .body(serde_json::to_string(&result).unwrap())
}

#[post("/v1/util/gpio_pin")]
async fn gpio_pin_picture() -> impl Responder {
    println!("=> GPIO 보드 사진");
    let mut result = Template::new();

    result.add_qr(QuickReply::new("gpio 사진", "보드 사진"));

    result.add_output(SimpleText::new("Raspberry Pi 3B+").build());
    result.add_output(
        SimpleImage::new(
            "https://seokwondev.s3.ap-northeast-2.amazonaws.com/gpio.jpg",
            "gpio_pin.png",
        )
        .build(),
    );

    HttpResponse::Ok()
        .content_type("aplication/json")
        .body(serde_json::to_string(&result).unwrap())
}

#[post("/v1/util/stat")]
async fn pi_stat() -> impl Responder {
    println!("=> 파이1 클라이언트 시스템 정보");
    let mut result = Template::new();

    // CPU 사용량
    let commands = vec![
        Exec::shell("grep 'cpu' /proc/stat"),
        Exec::shell(r#"awk '{usage=($2+$4)*100/($2+$4+$5)} END {printf "%.1f%%\n", usage}'"#),
    ]; // grep 'cpu' /proc/stat | awk '{usage=($2+$4)*100/($2+$4+$5)} END {printf "%.1f%%\n", usage}'

    let pipeline = subprocess::Pipeline::from_exec_iter(commands);
    let cpu_usage = pipeline.capture().unwrap().stdout_str();

    // WIFI 이름
    let wifi_name_cmd: Vec<&str> = r#"iwgetid -r"#.split(" ").map(|s| s).collect();
    let mut proc = Popen::create(
        &wifi_name_cmd,
        PopenConfig {
            stdout: Redirection::Pipe,
            stdin: Redirection::None,
            stderr: Redirection::None,
            ..Default::default()
        },
    )
    .unwrap();

    let (out, _) = proc.communicate(None).unwrap();
    let wifi_name = out.unwrap();

    // Memory 사용량
    let commands = vec![
        Exec::shell("free -h"),
        Exec::shell("grep Mem"),
        Exec::shell(r#"awk '{print $3, $7}'"#),
    ];

    let pipeline = subprocess::Pipeline::from_exec_iter(commands);
    let memory_usage = pipeline.capture().unwrap().stdout_str();
    let memory_usage = memory_usage.replace("Mi", "MB");
    let memories: Vec<&str> = memory_usage.split(" ").map(|s| s).collect();

    // 부팅 시간
    let commands = vec![Exec::shell("uptime -p"), Exec::shell("cut -d ' ' -f 2-")];
    let pipeline = subprocess::Pipeline::from_exec_iter(commands);
    let uptime = pipeline.capture().unwrap().stdout_str();

    let description = format!(
        "CPU 사용량: {}메모리 사용량: {}/{}WiFi 이름: {}부팅 시간: {}",
        cpu_usage, memories[0], memories[1], wifi_name, uptime
    );

    let basic_card = BasicCard::new()
        .set_title("[라즈베리파이 3B+]")
        .set_desc(description)
        .set_thumbnail(
            "https://www.notebookcheck.net/fileadmin/Notebooks/News/_nc3/Raspberry_Pi_Logo.jpg",
        )
        .add_button(
            Button::new(ButtonType::Link)
                .set_label("자세히")
                .set_link("https://www.raspberrypi.com/products/raspberry-pi-3-model-b-plus/"),
        );

    result.add_output(basic_card.build());

    HttpResponse::Ok()
        .content_type("application/json")
        .body(serde_json::to_string(&result).unwrap())
}

#[post("/bye")]
async fn bye(_: web::Json<Value>, data: web::Data<Mutex<TcpStream>>) -> impl Responder {
    println!("=> 감사합니다");

    let mut result = Template::new();
    result.add_qr(QuickReply::new("감사합니다", "감사합니다"));

    let mut data = data.lock().unwrap();

    // Buffer the bytes
    data.write_all("bye".as_bytes())
        .await
        .expect("sending goodbye");

    data.flush().await.expect("flush fail");

    HttpResponse::Ok()
        .content_type("aplication/json")
        .body(serde_json::to_string(&result).unwrap())
}
