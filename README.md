# 카카오파이

Using AWS S3 for photo storage, C language for socket communication, Python with OpenCV for face detection, and Rust for a chatbot server.

The project, `KakaoPi`, consists of three Raspberry Pi devices, each with its unique role.

The second Raspberry Pi acts as a TCP server and houses the Rust-based chatbot server, allowing for control of other devices through the KakaoTalk app.

The first Raspberry Pi acts as a TCP client and is equipped with a camera, buttons, LED lights, and sensors.

The third Raspberry Pi is also a TCP client, responsible for controlling an LCD screen.

With this setup, the Raspberry Pi Communication project is able to accomplish a wide range of tasks. 

For example, a message sent through the KakaoTalk app to the second Raspberry Pi's chatbot server could instruct the first Raspberry Pi to take a picture.

The picture is then uploaded to AWS S3, allowing for viewing from anywhere with internet access.

This project is a true example of mixing multiple programming languages (C + Py + Rust), wireless communication between pies and kakaotalk chatbot api to control pies by KakaoTalk.

[![kakaopi_video](https://img.youtube.com/vi/yu4VTjWsiuo/maxresdefault.jpg)](https://youtu.be/yu4VTjWsiuo)

![KakaoTalk_20220607_170805681](https://user-images.githubusercontent.com/2356749/172969187-4b86b295-89aa-4f79-9abb-0c8c2d58c65d.jpg)

![KakaoTalk_20220605_121608952_samsung-galaxys20-cosmicgrey-portrait](https://user-images.githubusercontent.com/2356749/172969281-7719462b-a98a-4c52-b57e-9bb377333433.png)

![image](https://user-images.githubusercontent.com/2356749/172969513-bfd4dd5d-cab5-4a06-9f2b-b2bec99f5616.png)

![image](https://user-images.githubusercontent.com/2356749/173337519-78fce863-2e0a-45ad-a6ce-c7cd43a43ba1.png)

카카오파이 소스입니다.

파이1 TCP 클라이언트, 파이2 챗봇에서의 TCP 클라이언트, 파이3 TCP 클라이언트

총 3개의 클라이언트와 파이2 TCP 서버로 이루어져 있습니다.

코드는 파이2는 C언어만, 파이1은 C언어(클라이언트) + 파이썬(파이카메라+OpenCV) + 파이3는 C언어만(클라이언트) 사용하였습니다.

```
파이1 - 카메라, LED, 조도센서, 초음파센서

파이2 - 센서 X

파이3 - LCD
```

## 전체 셋업

환경 변수 값들이 필요합니다.

`SERVER`: 서버 고정 IP

`ACCESS_KEY_ID`: AWS IAM 키

`ACCESS_SECRET_KEY`: AWS IAM 시크릿 키

`AWS_S3_BUCKET`: AWS S3 버킷 이름

## 파이1

![KakaoTalk_20220530_021223309](https://user-images.githubusercontent.com/2356749/172969346-bfd8e409-09cd-4c34-b07e-6a2970298054.jpg)

파이1은 C언어에서 Python을 컨트롤하기 위한

`Python C-API`를 사용했습니다.

카메라를 찍으라고 명령했을 때 (`사진 찍기 -> 마스크 감지 -> S3 업로드`)

위 과정만 파이썬으로 제작되었습니다.

### 실행

```sh
$ gcc -Wall client.c gpio.c pwm.c spi.c `pkg-config python3-embed --libs --cflags` -lpthread -o client
$ ./client
```

## 파이3

![KakaoTalk_20220607_171345773](https://user-images.githubusercontent.com/2356749/172969193-da4074c0-570b-407a-adc5-5616de9795f9.jpg)

파이3는 단순 LCD만 사용합니다.

조도 센서, 초음파 센서 등의 값들과 마지막 명령어를

LCD에 표시하기 위함입니다.

WiringPI C언어 라이브러리를 사용하였습니다.

### 실행

```sh
$ gcc -Wall client_lcd.c soft_lcd.c soft_i2c.c -lwiringPi -o client_lcd
$ ./client_lcd
```

## 파이2

외부 접속 가능하게 고정 ip가 필요합니다. (+ 포트포워딩)

개발용이기에 도메인 이름까지 사용안하고 ip 형식으로 주소가 이루어집니다.

https://123.456.789

카카오챗봇 Rust는 최초로 만들었습니다.

검색해도 제가 쓴 글밖에 안나옵니다. (아주대 챗봇)

kakao-rs 라이브러리도 만들어서 챗봇 서버 사용을 쉽게 하였습니다.

카카오챗봇 서버 (+ 웹 홈페이지) + TCP 서버 동시에 돌아감

![image](https://user-images.githubusercontent.com/2356749/173375915-422ac12a-89a6-456d-89c2-a153dd3927ad.png)

### 실행

아래로 서버를 빌드하고 실행

```sh
$ gcc -Wall server.c -o server
$ ./server
```

아래로 카카오파이 챗봇 서버 실행

```sh
$ cargo run --release
```

---

Made by 최석원 (Seok Won, Choi)
