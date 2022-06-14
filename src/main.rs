use actix_web::middleware::{Compress, Logger, NormalizePath};
use actix_web::{get, http, post, web, App, HttpResponse, HttpServer, Responder};

use async_std::net::{Shutdown, TcpStream};
use async_std::prelude::*;

use std::process::exit;
use std::sync::Mutex;

use mime_guess::from_path;
use rust_embed::RustEmbed;
use serde::{Deserialize, Serialize};
use tera::{Context, Tera};

#[derive(RustEmbed)]
#[folder = "templates/"]
struct Asset;

#[derive(Serialize, Deserialize)]
pub struct LCDParam {
    lcd: String,
}

fn redirect_to(location: &str) -> HttpResponse {
    HttpResponse::Found()
        .append_header((http::header::LOCATION, location))
        .finish()
}

/// Simple handle POST request
#[post("/lcd_post")]
async fn html_lcd_post(
    data: web::Data<Mutex<TcpStream>>,
    params: web::Form<LCDParam>,
) -> impl Responder {
    println!("=> HTML LCD: {}", params.lcd);
    let mut data = data.lock().unwrap();

    let action_tcp = format!("lcd {}", params.lcd);

    // Buffer the bytes
    data.write_all(&action_tcp.as_bytes())
        .await
        .expect("sending msg on html lcd");

    data.flush().await.expect("flush fail");

    redirect_to("/lcd_home")
}

fn handle_embedded_file(path: &str) -> impl Responder {
    match Asset::get(path) {
        Some(content) => HttpResponse::Ok()
            .content_type(from_path(path).first_or_octet_stream().as_ref())
            .body(content.data.into_owned()),
        None => HttpResponse::NotFound().body("404 Not Found"),
    }
}

#[get("/")]
async fn hello() -> impl Responder {
    HttpResponse::Ok()
        .content_type("aplication/json")
        .body(r#"{"state": "welcome"}"#)
}

#[post("/kill")]
async fn kill(data: web::Data<Mutex<TcpStream>>) -> impl Responder {
    println!("===== 챗봇 서버 종료 =====");
    let mut data = data.lock().unwrap();

    data.write_all("kill".as_bytes())
        .await
        .expect("sending msg");

    data.flush().await.expect("flush fail");

    data.shutdown(Shutdown::Both).expect("shutdown call failed");

    exit(0); // 파이1 클라이언트 + 파이2 카톡 클라이언트 같이 종료

    #[allow(unreachable_code)]
    HttpResponse::Ok()
        .content_type("aplication/json")
        .body(r#"{"state": "kill"}"#)
}

#[get("/lcd_home")]
async fn html_lcd(data: web::Data<Mutex<Tera>>) -> impl Responder {
    let mut ctx = Context::new();
    ctx.insert("name", "test");

    let mut data = data.lock().unwrap();

    let index_html = Asset::get("lcd.html").unwrap();
    let index_html = std::str::from_utf8(index_html.data.as_ref()).unwrap();

    let rendered = data.render_str(index_html, &ctx).unwrap();
    HttpResponse::Ok().content_type("text/html").body(rendered)
}

#[actix_web::main]
async fn main() -> std::io::Result<()> {
    println!("===== 파이2 카카오 챗봇 서버 =====");
    let stream = TcpStream::connect(pi_kakao::TCP_SERVER)
        .await
        .expect("connect 실패");

    let data = web::Data::new(Mutex::new(stream));

    let tera = web::Data::new(Mutex::new(Tera::new("/templates/**/*.html").unwrap()));

    // 서버 실행
    HttpServer::new(move || {
        App::new()
            .app_data(data.clone())
            .app_data(tera.clone())
            // 미들웨어
            .wrap(Compress::default())
            .wrap(NormalizePath::default())
            .wrap(Logger::default())
            // 홈페이지
            .service(html_lcd)
            .service(html_lcd_post)
            // 서버 체크 GET
            .service(hello)
            .service(kill)
            // 센서 POST
            .service(pi_kakao::sensor::led_control)
            .service(pi_kakao::sensor::led_control_pwm)
            .service(pi_kakao::sensor::lcd_control)
            .service(pi_kakao::sensor::spi_get_light)
            .service(pi_kakao::sensor::control_ultrasonic)
            .service(pi_kakao::sensor::camera_control)
            .service(pi_kakao::sensor::camera_picture)
            // .service(pi_kakao::sensor::camera_live)
            // 유틸 POST
            .service(pi_kakao::util::run_linux_cmd)
            .service(pi_kakao::util::gpio_readall)
            .service(pi_kakao::util::gpio_pin_picture)
            .service(pi_kakao::util::pi_stat)
            .service(pi_kakao::util::bye)
    })
    .bind(pi_kakao::SERVER)?
    .run()
    .await
}
