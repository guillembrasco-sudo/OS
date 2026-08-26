#ifndef HAL_CLOCK_H
#define HAL_CLOCK_H

#include <stdint.h>

void clock_init(void);
void clock_tick(uint32_t milliseconds);
uint64_t clock_uptime_ms(void);

struct clock_datetime {
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
};

int clock_read_rtc(struct clock_datetime *datetime);

#endif