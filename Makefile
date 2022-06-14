TARGET	= main
CFLAGS	= 
LDFLAGS	=

pi1:
	gcc -Wall client.c gpio.c pwm.c spi.c `pkg-config python3-embed --libs --cflags` -lpthread -o client

pi2:
	gcc -Wall server.c -o server

pi3:
	gcc -Wall client_lcd.c soft_lcd.c soft_i2c.c -lwiringPi -o client_lcd

.PHONY: clean
clean:
	rm -rf $(TARGET) *.o *.dSYM