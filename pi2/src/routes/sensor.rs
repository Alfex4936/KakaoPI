#![allow(proc_macro_derive_resolution_fallback)]
use actix_web::{post, web, HttpResponse, Responder};
use async_std::net::TcpStream;
use async_std::prelude::*;
use kakao_rs::prelude::*;
use serde_json::Value;
use std::sync::Mutex;

#[post("/v1/led")]
async fn led_control(kakao: web::Json<Value>, data: web::Data<Mutex<TcpStream>>) -> impl Responder {
    println!("=> LED 컨트롤");
    let mut result = Template::new();

    let kakao_action = &kakao["action"]["params"];
    let action = match kakao_action.get("action") {
        Some(v) => v.as_str().unwrap(),
        _ => {
            result.add_qr(QuickReply::new("led on", "led on"));
            result.add_qr(QuickReply::new("led off", "led off"));
            result.add_output(SimpleText::new("명령어를 확인해주세요.").build());

            return HttpResponse::Ok()
                .content_type("application/json")
                .body(serde_json::to_string(&result).unwrap());
        }
    };

    let mut data = data.lock().unwrap();

    // Buffer the bytes
    data.write_all(action.as_bytes())
        .await
        .expect("sending msg");

    data.flush().await.expect("flush fail");

    result.add_qr(QuickReply::new("led 켜기", "led on"));
    result.add_qr(QuickReply::new("led 끄기", "led off"));

    let simple_text: SimpleText = if action.eq("on") {
        SimpleText::new("LED를 켭니다.")
    } else {
        SimpleText::new("LED를 끕니다.")
    };

    result.add_output(simple_text.build());

    HttpResponse::Ok()
        .content_type("aplication/json")
        .body(serde_json::to_string(&result).unwrap())
}

#[post("/v1/lcd")]
async fn lcd_control(kakao: web::Json<Value>, data: web::Data<Mutex<TcpStream>>) -> impl Responder {
    println!("=> LCD 컨트롤");
    let mut result = Template::new();

    let kakao_action = &kakao["action"]["params"];
    let action = match kakao_action.get("text") {
        Some(v) => v.as_str().unwrap(),
        _ => {
            result.add_qr(QuickReply::new("lcd", "lcd"));
            result.add_output(SimpleText::new("명령어를 확인해주세요.").build());

            return HttpResponse::Ok()
                .content_type("application/json")
                .body(serde_json::to_string(&result).unwrap());
        }
    };

    let mut data = data.lock().unwrap();

    let action_tcp = format!("lcd {}", action);

    // Buffer the bytes
    data.write_all(&action_tcp.as_bytes())
        .await
        .expect("sending msg");

    data.flush().await.expect("flush fail");

    result.add_qr(QuickReply::new("lcd", "lcd"));

    let simple_text: SimpleText = SimpleText::new("LCD 패널 텍스트 변경");
    let simple_text2: SimpleText = SimpleText::new(format!("{}", action));

    result.add_output(simple_text.build());
    result.add_output(simple_text2.build());

    HttpResponse::Ok()
        .content_type("aplication/json")
        .body(serde_json::to_string(&result).unwrap())
}

#[post("/v1/led_pwm")]
async fn led_control_pwm(
    kakao: web::Json<Value>,
    data: web::Data<Mutex<TcpStream>>,
) -> impl Responder {
    println!("=> LED PWM 값 조절");
    let mut result = Template::new();

    let kakao_action = &kakao["action"]["params"];
    let action = match kakao_action.get("value") {
        Some(v) => v.as_str().unwrap(),
        _ => {
            result.add_qr(QuickReply::new("led 밝기 조절", "led 밝기 조절"));
            result.add_output(SimpleText::new("명령어를 확인해주세요.").build());

            return HttpResponse::Ok()
                .content_type("application/json")
                .body(serde_json::to_string(&result).unwrap());
        }
    };

    let float = action.parse::<f64>().unwrap();

    let mut data = data.lock().unwrap();

    let action = format!("pwm {}", action);

    // Buffer the bytes
    data.write_all(&action.as_bytes())
        .await
        .expect("sending msg");

    data.flush().await.expect("flush fail");

    result.add_qr(QuickReply::new("led 밝기 조절", "led 밝기 조절"));

    let simple_text: SimpleText = SimpleText::new(format!("밝기 조절: {:.1}%", float * 100 as f64));

    result.add_output(simple_text.build());

    HttpResponse::Ok()
        .content_type("aplication/json")
        .body(serde_json::to_string(&result).unwrap())
}

#[post("/v1/ultrasonic")]
async fn control_ultrasonic(
    _: web::Json<Value>,
    data: web::Data<Mutex<TcpStream>>,
) -> impl Responder {
    println!("=> 초음파 센서");
    let mut result = Template::new();

    let mut data = data.lock().unwrap();

    // Buffer the bytes
    data.write_all("sonic".as_bytes())
        .await
        .expect("sending msg");

    data.flush().await.expect("flush fail");

    result.add_qr(QuickReply::new("초음파 센서", "초음파"));

    let simple_text: SimpleText = SimpleText::new("초음파 센서 값에 따라 LED를 변화시킵니다.");

    result.add_output(simple_text.build());

    HttpResponse::Ok()
        .content_type("aplication/json")
        .body(serde_json::to_string(&result).unwrap())
}

#[post("/v1/spi_light")]
async fn spi_get_light(
    kakao: web::Json<Value>,
    data: web::Data<Mutex<TcpStream>>,
) -> impl Responder {
    println!("=> SPI 조도센서");
    let mut result = Template::new();

    let kakao_action = &kakao["action"]["params"];
    let action = match kakao_action.get("action") {
        Some(v) => v.as_str().unwrap(),
        _ => {
            result.add_qr(QuickReply::new("spi light", "spi light"));
            result.add_output(SimpleText::new("명령어를 확인해주세요.").build());

            return HttpResponse::Ok()
                .content_type("application/json")
                .body(serde_json::to_string(&result).unwrap());
        }
    };

    let mut data = data.lock().unwrap();

    // Buffer the bytes
    data.write_all(action.as_bytes())
        .await
        .expect("sending msg");

    data.flush().await.expect("flush fail");

    result.add_qr(QuickReply::new("spi light", "spi light"));

    result.add_output(SimpleText::new("SPI 조도센서 값에 따라 LED 값을 변화시킵니다.").build());

    HttpResponse::Ok()
        .content_type("aplication/json")
        .body(serde_json::to_string(&result).unwrap())
}

#[post("/v1/camera")]
async fn camera_control(
    kakao: web::Json<Value>,
    data: web::Data<Mutex<TcpStream>>,
) -> impl Responder {
    println!("=> 사진/동영상 찍기");
    let mut result = Template::new();

    let kakao_action = &kakao["action"]["params"];
    let action = match kakao_action.get("action") {
        Some(v) => v.as_str().unwrap(),
        _ => {
            result.add_qr(QuickReply::new("사진 찍어", "사진 찍어"));
            result.add_output(SimpleText::new("명령어를 확인해주세요.").build());

            return HttpResponse::Ok()
                .content_type("application/json")
                .body(serde_json::to_string(&result).unwrap());
        }
    };

    let mut data = data.lock().unwrap();

    // Buffer the bytes
    data.write_all(action.as_bytes())
        .await
        .expect("sending msg on camera");

    data.flush().await.expect("flush fail");

    // data.shutdown(Shutdown::Both).expect("shutdown call failed");

    result.add_qr(QuickReply::new("지난 사진 보기", "사진 보여줘"));
    result.add_qr(QuickReply::new("사진 찍기", "사진"));
    result.add_qr(QuickReply::new("동영상 찍기", "동영상"));

    let simple_text: SimpleText = if action.eq("picture") {
        SimpleText::new("사진을 찍습니다.")
    } else {
        SimpleText::new("동영상을 찍습니다.")
    };

    result.add_output(simple_text.build());

    HttpResponse::Ok()
        .content_type("aplication/json")
        .body(serde_json::to_string(&result).unwrap())
}

#[post("/v1/camera_view")]
async fn camera_picture(_: web::Json<Value>, _: web::Data<Mutex<TcpStream>>) -> impl Responder {
    println!("=> 사진 로딩하기");
    let mut result = Template::new();

    result.add_qr(QuickReply::new("사진 찍기", "사진"));

    result.add_output(SimpleText::new("마지막 사진을 불러옵니다.").build());
    result.add_output(
        SimpleImage::new(
            "https://seokwondev.s3.ap-northeast-2.amazonaws.com/random.png",
            "PiCamera",
        )
        .build(),
    );

    HttpResponse::Ok()
        .content_type("aplication/json")
        .body(serde_json::to_string(&result).unwrap())
}

#[post("/v1/camera_live")]
async fn camera_live(kakao: web::Json<Value>, data: web::Data<Mutex<TcpStream>>) -> impl Responder {
    println!("=> 카메라 라이브");
    // 와이파이 ip 얻기, 근데 클라에서 해야하는데...
    // let commands = vec![
    //     Exec::shell("ip -4 addr show wlan0"),
    //     Exec::shell(r#"grep -oP '(?<=inet\s)\d+(\.\d+){3}'"#),
    // ];

    // let pipeline = subprocess::Pipeline::from_exec_iter(commands);
    // let wifi_ip = pipeline.capture().unwrap().stdout_str();

    let mut result = Template::new();
    result.add_qr(QuickReply::new("라이브 보기", "라이브 보기"));

    let basic_card = BasicCard::new()
        .set_title("카메라 라이브")
        .set_desc("홈페이지로 연결됩니다.")
        .set_thumbnail("https://www.raspberrypi.org/app/uploads/2017/06/Powered-by-Raspberry-Pi-Logo_Outline-Colour-Screen-500x153.png")
        .add_button(
            Button::new(ButtonType::Link)
                .set_label("열기")
                .set_link("http://카메라서버:8989/index.html"),
        );

    // result.add_output(SimpleText::new("카메라 라이브 주소입니다.").build());

    result.add_output(basic_card.build());

    let kakao_action = &kakao["action"]["params"];
    let action = match kakao_action.get("action") {
        Some(v) => v.as_str().unwrap(),
        _ => {
            result.add_qr(QuickReply::new("라이브 보기", "라이브"));
            result.add_output(SimpleText::new("명령어를 확인해주세요.").build());

            return HttpResponse::Ok()
                .content_type("application/json")
                .body(serde_json::to_string(&result).unwrap());
        }
    };
    let mut data = data.lock().unwrap();

    // Buffer the bytes
    data.write_all(action.as_bytes())
        .await
        .expect("sending msg on camera");

    data.flush().await.expect("flush fail");

    HttpResponse::Ok()
        .content_type("aplication/json")
        .body(serde_json::to_string(&result).unwrap())
}
