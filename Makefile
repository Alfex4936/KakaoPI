TARGET	= main
CFLAGS	= 
LDFLAGS	=

pi1:
	gcc -Wall pi1/client.c gpio.c pwm.c spi.c `pkg-config python3-embed --libs --cflags` -lpthread -o binary/client

pi2:
	gcc -Wall pi2/server.c -o binary/server

pi3:
	gcc -Wall pi3/client_lcd.c pi3/soft_lcd.c pi3/soft_i2c.c -lwiringPi -o binary/client_lcd

.PHONY: clean
clean:
	rm -rf $(TARGET) *.o *.dSYM