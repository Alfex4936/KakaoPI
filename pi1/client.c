#include <Python.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// #define PORT 8888
#define PORT 11114

#include "gpio.h"
#include "pwm.h"
#include "spi.h"

#define IN 0
#define OUT 1
#define PWM 2
#define LOW 0
#define HIGH 1

#define LED_PIN 20
#define LED_POUT 18

#define BUTTON_IN 20
#define BUTTON_OUT 21

#define ULTRASONIC_POUT 23
#define ULTRASONIC_PIN 24

#define D 35

const char *SPI_DEVICE_0 = "/dev/spidev0.0";
int sock = 0;

void *main2()
{
    printf("\n-------- Main thread\n");

    const char *SERVER_IP = getenv("SERVER"); // 외부에서 접속 가능한 IP

    //********** Python C-API
    PyObject *sys, *path;
    PyObject *pName, *pModule, *pDict, *pClass, *object, *pResult;

    Py_Initialize();

    sys = PyImport_ImportModule("sys");
    path = PyObject_GetAttrString(sys, "path");

    PyList_Append(path, PyUnicode_FromString("."));

    // import mypi.py
    pName = PyUnicode_FromString((char *)"mypi");
    if (!pName)
    {
        PyErr_Print();
        printf("Error finding mypi.py file in sys.path\n");
    }

    // 모듈 import
    pModule = PyImport_Import(pName);
    if (!pModule)
    {
        PyErr_Print();
        printf("Error importing python script.\n");
    }

    pDict = PyModule_GetDict(pModule);

    // pClass는 helper.py -> MyPi 클래스
    pClass = PyDict_GetItemString(pDict, (char *)"MyPi");
    if (pClass && PyCallable_Check(pClass))
    {
        object = PyObject_CallObject(pClass, NULL);
    }
    else
    {
        printf("Error calling MyPi class\n");
        PyErr_Print();
    }
    //********** Python C-API

    //********** PWM 파트 시작
    pwm_t *pwm1;
    pwm1 = pwm_new();
    /* PWM chip0, 채널0 */
    if (pwm_open(pwm1, 0, 0) < 0)
    {
        fprintf(stderr, "pwm_open(): %s\n", pwm_errmsg(pwm1));
        exit(1);
    }

    if (pwm_set_frequency(pwm1, 1e3) < 0)
    {
        fprintf(stderr, "pwm_set_frequency(): %s\n", pwm_errmsg(pwm1));
        exit(1);
    }

    /* Enable PWM */
    if (pwm_enable(pwm1) < 0)
    {
        fprintf(stderr, "pwm_enable(): %s\n", pwm_errmsg(pwm1));
        exit(1);
    }

    //********** PWM 파트 종료

    //********** SPI 파트 시작
    int light = 0;

    int spi_fd = open(SPI_DEVICE_0, O_RDWR);
    if (spi_fd <= 0)
    {
        printf("Device %s not found\n", SPI_DEVICE_0);
        exit(1);
    }

    if (prepare(spi_fd) == -1)
    {
        exit(1);
    }
    //********** SPI 파트 종료

    //********** 초음파센서 파트 종료
    clock_t start_t, end_t;
    double time, distance;

    if (GPIOExport(ULTRASONIC_POUT) == -1 || GPIOExport(ULTRASONIC_PIN) == -1)
    {
        printf("gpio export err\n");
        exit(1);
    }

    usleep(2000000);

    if (GPIODirection(ULTRASONIC_POUT, OUT) == -1 ||
        GPIODirection(ULTRASONIC_PIN, IN) == -1)
    {
        printf("gpio direction err\n");
        exit(1);
    }

    usleep(10000);
    GPIOWrite(ULTRASONIC_POUT, 0);

    //********** 초음파센서 파트 종료

    //********** 소켓 설정 시작
    // struct hostent *hostnm;
    // hostnm = gethostbyname("www.naver.com");
    // if (hostnm == (struct hostent *)0)
    // {
    //     fprintf(stderr, "Gethostbyname failed\n");
    //     exit(2);
    // }

    int valread, client_fd;
    struct sockaddr_in serv_addr;

    char buffer[1025] = {
        '\0',
    };
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    // serv_addr.sin_addr.s_addr = *((unsigned long *)hostnm->h_addr);

    // IPv4|IPv6 주소 텍스트 -> 바이너리
    if (inet_pton(PF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        exit(1);
    }

    if ((client_fd = connect(sock, (struct sockaddr *)&serv_addr,
                             sizeof(serv_addr))) < 0)
    {
        printf("\nConnection Failed \n");
        exit(1);
    }

    //********** 소켓 설정 종료

    //********** 메인 while
    while (1)
    {
        memset(buffer, 0, strlen(buffer)); // buffer 초기화
        // send(sock, give_me, strlen(give_me), 0);
        valread = read(sock, buffer, 1024); // hang
        if (valread == -1)
        {
            usleep(5000);
            continue;
        }
        printf("\n=== Received message from PI 2 : %s\n", buffer);

        if (strncmp("on", buffer, 2) == 0) // led light 최대
        {
            printf("Turning on light...\n");
            pwm_set_duty_cycle(pwm1, 0.0);
            usleep(1000);
            pwm_set_duty_cycle(pwm1, 1.0);
        }
        else if (strncmp("off", buffer, 3) == 0) // led light X
        {
            printf("Turning off light...\n");
            pwm_set_duty_cycle(pwm1, 0.0);
        }
        else if (strncmp("picture", buffer, 7) == 0)
        {
            // 사진 찍기
            // 과정: led 켜져 있으면 끄기 -> 사진 찍기 -> 사진 OpenCV 처리 ->
            // 사진 업로드 -> LED blink
            memset(buffer, 0, strlen(buffer)); // buffer 초기화
            printf("C: Taking a picture...\n");
            pwm_set_duty_cycle(pwm1, 0.0);
            pwm_set_duty_cycle(pwm1, 0.0);
            pResult = PyObject_CallMethod(object, "take_picture", NULL);
            Py_DECREF(pResult); // ref--
            printf("C: Trying to upload a picture...\n");
            pResult = PyObject_CallMethod(object, "upload_picture", NULL);
            Py_DECREF(pResult); // ref--

            // LCD 출력용
            char hello[9] = "print_pic";
            send(sock, hello, strlen(hello), 0);

            pwm_set_duty_cycle(pwm1, 1.0);
            sleep(1);
            pwm_set_duty_cycle(pwm1, 0.0);
            sleep(1);
            pwm_set_duty_cycle(pwm1, 1.0);
            sleep(1);
            pwm_set_duty_cycle(pwm1, 0.0);
            // PyObject *pLink = PyUnicode_AsEncodedString(pResult, "utf-8",
            // "strict"); char *image_link = PyBytes_AsString(pLink);
            // Py_DECREF(pResult); // ref--
            // send(sock, image_link, strlen(image_link), 0);
        }
        else if (strncmp("video", buffer, 5) == 0) // 5초간 동영상 찍기
        {
            printf("C: Taking a video...\n");
            pResult = PyObject_CallMethod(object, "take_video", NULL);
            Py_DECREF(pResult); // ref--
        }
        // else if (strncmp("live", buffer, 4) == 0)  // 라이브 홈페이지 열기
        // {
        //     printf("C: Serving streaming website...\n");
        //     pResult = PyObject_CallMethod(object, "do_live", NULL);
        //     printf("C: Streaming done!");
        //     Py_DECREF(pResult); // ref--
        // }
        else if (strncmp(buffer, "pwm", 3) == 0)
        {
            // pwm % 설정
            // 카카오톡에서 [0.0 ~ 1.0] 사이 실수 입력할 수 있음
            char value[3]; // 0.0 ~ 1.0
            strncpy(value, buffer + 4, 3);

            // LCD 출력용
            char hello[13] = "print_pwm ";
            strncpy(hello + 10, value, 3);
            send(sock, hello, strlen(hello), 0);

            char *error;
            double pwm_value = strtod(value, &error);
            // if (!isspace((unsigned char)*error))
            // {
            //     // strtod 에러 시 그냥 LED light 0
            //     pwm_value = 0.0;
            // }
            pwm_set_duty_cycle(pwm1, 0.0);

            if (pwm_value < 0.0)
            {
                pwm_value = 0.0;
            }
            else if (pwm_value > 1.0)
            {
                pwm_value = 1.0;
            }
            // printf("Float: %f\n", strtod(value, NULL));
            pwm_set_duty_cycle(pwm1, pwm_value);
        }
        else if (strncmp(buffer, "light", 5) ==
                 0) // SPI 조도 센서 값 읽고 LED 변경
        {
            light = readadc(spi_fd, 0);
            printf("spi light: %d\n", light);
            pwm_set_duty_cycle_ns(pwm1, light * 750);

            char value[3];
            sprintf(value, "%d", light);

            // LCD 출력용
            char hello[13] = "print_spi ";
            strncpy(hello + 10, value, strlen(value));
            send(sock, hello, strlen(hello), 0);
        }
        else if (strncmp(buffer, "sonic", 5) == 0)
        {
            if (GPIOWrite(ULTRASONIC_POUT, 1) == -1)
            {
                printf("gpio write/trigger err\n");
                break;
            }

            usleep(100);
            GPIOWrite(ULTRASONIC_POUT, 0);

            while (GPIORead(ULTRASONIC_PIN) == 0)
            {
                start_t = clock();
            }

            while (GPIORead(ULTRASONIC_PIN) == 1)
            {
                end_t = clock();
            }

            time = (double)(end_t - start_t) / CLOCKS_PER_SEC;
            if (time < 0.0)
            {
                time = 0.0001;
            }

            distance = time / 2 * 34000;
            if (distance > 900)
            {
                distance = 900.0;
            }
            else if (distance < 0)
            {
                distance = 0.0;
            }

            char value[3];
            sprintf(value, "%d", (int)distance);

            // LCD 출력용
            char hello[13] = "print_ult ";
            strncpy(hello + 10, value, strlen(value));
            send(sock, hello, strlen(hello), 0);

            printf("distance : %.2lfcm\n", distance);

            if (distance > D)
            {
                pwm_set_duty_cycle(pwm1, 0.0);
            }
            else
            {
                pwm_set_duty_cycle(pwm1, 1.0);
            }
        }
        else if (strncmp(buffer, "kill", 4) == 0)
        {
            // 개발용 서버 종료 커맨드
            // POST http://server/kill
            break;
        }
    }

    // 종료 Py3+
    Py_DECREF(sys);
    Py_DECREF(path);
    Py_DECREF(object);

    Py_FinalizeEx();

    close(client_fd);
    close(spi_fd);

    pwm_close(pwm1);
    pwm_free(pwm1);

    if (GPIOUnexport(BUTTON_OUT) == -1 || GPIOUnexport(BUTTON_IN) == -1)
    {
        printf("gpio export err\n");
        exit(1);
    }

    if (GPIOUnexport(ULTRASONIC_POUT) == -1 ||
        GPIOUnexport(ULTRASONIC_PIN) == -1)
    {
        printf("gpio export err\n");
        exit(1);
    }

    exit(0);
}

void *btn_thread()
{
    printf("-------- Button thread\n");
    char *plz = "picture";

    //********** BUTTON
    // BUTTON START
    if (GPIOExport(BUTTON_OUT) == -1 || GPIOExport(BUTTON_IN) == -1)
    {
        exit(1);
    }

    usleep(2000000); // direction 시간 줌, 안주면 direction error

    if (GPIODirection(BUTTON_OUT, OUT) == -1 ||
        GPIODirection(BUTTON_IN, IN) == -1)
    {
        exit(1);
    }
    // BUTTON END
    //********** BUTTON

    int pressed = 0;

    while (1)
    {
        if (GPIOWrite(BUTTON_OUT, 1) == -1)
        {
            break;
        }

        usleep(10000);

        if (GPIORead(BUTTON_IN) == 0)
        { // clicked

            if (!pressed)
            {
                send(sock, plz, strlen(plz), 0);
                pressed = 1;
            }
            else
            {
                pressed = 0;
            }
            usleep(500000);
            // light = readadc(spi_fd, 0);
            // printf("button light: %d\n", light);
            // pwm_set_duty_cycle_ns(pwm1, light * 750);
        }
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    // 버튼 read하는 쓰레드랑 main 쓰레드랑 나눠야 했음
    pthread_t p_thread[2];
    int thr_id;
    int status;

    char p1[] = "thread_main";
    char p2[] = "thread_button";

    thr_id = pthread_create(&p_thread[0], NULL, main2, (void *)p1);
    if (thr_id < 0)
    {
        perror("main thread create error");
        exit(0);
    }
    thr_id = pthread_create(&p_thread[1], NULL, btn_thread, (void *)p2);
    if (thr_id < 0)
    {
        perror("btn thread create error");
        exit(0);
    }
    pthread_join(p_thread[0], (void **)&status);
    pthread_join(p_thread[1], (void **)&status);

    return 0;
}
