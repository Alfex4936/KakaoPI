#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "soft_lcd.h"

// #define PORT 8888
#define PORT 11114
int sock = 0;

// 문자 "ㅋ"
char char_k[8] = {
    0b11111, 0b00001, 0b00001, 0b11111, 0b00001, 0b00001, 0b00001, 0b00001};

// 문자 "ㅏ"
char char_a[8] = {
    0b00100, 0b00100, 0b00100, 0b00111, 0b00100, 0b00100, 0b00100, 0b00100};

// 문자 "오"
char char_oh[8] = {
    0b00100, 0b01010, 0b10001, 0b01010, 0b00100, 0b00000, 0b00100, 0b11111};

// 문자 "ㅍ"
char char_f[8] = {
    0b00000, 0b11111, 0b01010, 0b01010, 0b01010, 0b01010, 0b11111, 0b00000};

// 문자 "ㅇ"
char char_e[8] = {
    0b00000,
    0b00110,
    0b01001,
    0b01001,
    0b01001,
    0b01001,
    0b00110,
    0b00000};

// 문자 "ㅣ"
char char_i[8] = {
    0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100};

// 문자 "♥"
char char_heart[8] = {
    0b00000,
    0b01010,
    0b11111,
    0b11111,
    0b11111,
    0b01110,
    0b00100,
    0b00000};

// 문자 "■"
char char_full[8] = {
    0b11111,
    0b11111,
    0b11111,
    0b11111,
    0b11111,
    0b11111,
    0b11111,
    0b11111};

void print_kakaopi(lcd_t *lcd)
{
    lcd_pos(lcd, 1, 6);

    lcd_create_char(lcd, 1, char_k);     // ㅋ
    lcd_create_char(lcd, 2, char_a);     // ㅏ
    lcd_create_char(lcd, 3, char_oh);    // 오
    lcd_create_char(lcd, 4, char_f);     // ㅍ
    lcd_create_char(lcd, 5, char_i);     // ㅣ
    lcd_create_char(lcd, 6, char_e);     // ㅇ
    lcd_create_char(lcd, 7, char_heart); // 하트

    lcd_print(lcd, "\01");
    lcd_print(lcd, "\02");
    lcd_print(lcd, "\01");
    lcd_print(lcd, "\02");
    lcd_print(lcd, "\03");
    lcd_print(lcd, "\04");
    lcd_print(lcd, "\02");
    lcd_print(lcd, "\06");
    lcd_print(lcd, "\05");
    lcd_print(lcd, "\07");
}

int main(void)
{
    printf("\n======== KakaoPI RaspberryPI 3 for LCD client\n");
    const char *SERVER_IP = getenv("SERVER"); // 외부에서 접속 가능한 IP

    //********** LCD 설정
    lcd_t *lcd = lcd_create(9, 8, 0x27, 3);
    // lcd->replace_UTF8_chars = 0;  // don't replace chars

    if (lcd == NULL)
    {
        printf("Cannot set-up LCD\n");
        return 1;
    }
    usleep(10000);

    // 카카오파이 커스텀 출력
    print_kakaopi(lcd);

    char lcd_text[80];
    //********** 소켓 설정 시작
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

    sleep(1);

    // 20*4
    // lcd_create_char(lcd, 7, char_full); // 네모
    // for (int row = 0; row < 5; row++) {
    //     for (int col = 0; col < 21; col++) {
    //         lcd_clear(lcd);
    //         lcd_pos(lcd, row, col);
    //         lcd_print(lcd, "\07");
    //         usleep(50000);
    //     }
    // }

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
        printf("\n===PI3: Received message from PI 2 : %s\n", buffer);

        // lcd 부분
        if (strncmp(buffer, "lcd", 3) == 0)
        {
            strncpy(lcd_text, buffer + 4, sizeof(lcd_text));
            lcd_clear(lcd);
            // lcd_pos(lcd,1, 6);
            lcd_print(lcd, lcd_text);
            // lcd_print(lcd, "\07");  // 하트 추가
            // sleep(1);
        }
        else if (strncmp(buffer, "on", 2) == 0)
        {
            lcd_clear(lcd);
            lcd_pos(lcd, 1, 7);
            lcd_print(lcd, "LED ON");
        }
        else if (strncmp(buffer, "off", 3) == 0)
        {
            lcd_clear(lcd);
            lcd_pos(lcd, 1, 7);
            lcd_print(lcd, "LED OFF");
        }
        else if (strncmp(buffer, "pwm", 3) == 0)
        {
            lcd_clear(lcd);
            lcd_pos(lcd, 1, 1);
            lcd_print(lcd, "PWM Duty Cycle: ");
        }
        else if (strncmp(buffer, "light", 5) == 0)
        {
            lcd_clear(lcd);
            lcd_pos(lcd, 1, 2);
            lcd_print(lcd, "SPI light: ");
        }
        else if (strncmp(buffer, "picture", 7) == 0)
        {
            lcd_clear(lcd);
            lcd_pos(lcd, 1, 2);
            lcd_print(lcd, "Taking a picture");
        }
        else if (strncmp(buffer, "sonic", 5) == 0)
        {
            lcd_clear(lcd);
            lcd_pos(lcd, 1, 1);
            lcd_print(lcd, "Ultrasonic: ");
        }
        else if (strncmp(buffer, "print_pwm", 9) == 0)
        {
            lcd_print(lcd, buffer + 10);
        }
        else if (strncmp(buffer, "print_spi", 9) == 0)
        {
            lcd_print(lcd, buffer + 10);
            lcd_print(lcd, " Lx");
        }
        else if (strncmp(buffer, "print_ult", 9) == 0)
        {
            lcd_print(lcd, buffer + 10);
            lcd_print(lcd, "cm");
        }
        else if (strncmp(buffer, "print_pic", 9) == 0)
        {
            lcd_clear(lcd);
            lcd_pos(lcd, 1, 3);
            lcd_print(lcd, "Took a picture");
        }
        else if (strncmp(buffer, "bye", 3) == 0)
        {
            lcd_clear(lcd);
            print_kakaopi(lcd);
        }
        else if (strncmp(buffer, "kill", 4) == 0)
        {
            // 개발용 서버 종료 커맨드
            // POST http://server/kill
            break;
        }
        memset(buffer, 0, strlen(buffer)); // buffer 초기화
    }

    // 종료
    close(client_fd);

    lcd_destroy(lcd);
    return 0;
}
