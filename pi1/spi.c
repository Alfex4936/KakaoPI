#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "spi.h"

#define ARRAY_SIZE(array) sizeof(array) / sizeof(array[0])

uint8_t MODE = SPI_MODE_0;
uint8_t BITS = 8;
uint32_t CLOCK = 1000000;
uint16_t DELAY = 5;

int prepare(int fd)
{
    if (ioctl(fd, SPI_IOC_WR_MODE, &MODE) == -1)
    {
        perror("Cant set MODE\n");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &BITS) == -1)
    {
        perror("Cant set number of bits\n");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &CLOCK) == -1)
    {
        perror("Cant set write clock\n");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &CLOCK) == -1)
    {
        perror("Cant set read clock\n");
        return -1;
    }

    return 0;
}

uint8_t control_bits_differential(uint8_t channel)
{
    return (channel & 7) << 4;
}

uint8_t control_bits(uint8_t channel)
{
    return 0x8 | control_bits_differential(channel);
}

int readadc(int fd, uint8_t channel)
{
    uint8_t tx[] = {1, control_bits(channel), 0};
    uint8_t rx[3];

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx),
        .delay_usecs = DELAY,
        .speed_hz = CLOCK,
        .bits_per_word = BITS,
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) == 1)
    {
        perror("IO Error\n");
        abort();
    }

    return (((rx[1] << 8) & 0x300) | (rx[2] & 0xFF));
}