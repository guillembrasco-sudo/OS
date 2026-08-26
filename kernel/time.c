#include <stdint.h>
#include <hal/clock.h>
#if defined(ARCH_x86_64)
#include <arch/x86_64/io.h>
#endif

static uint64_t uptime_milliseconds;

void clock_init(void)
{
	uptime_milliseconds = 0;
}

void clock_tick(uint32_t milliseconds)
{
	uptime_milliseconds += milliseconds;
}

uint64_t clock_uptime_ms(void)
{
	return uptime_milliseconds;
}

#if defined(ARCH_x86_64)
static uint8_t rtc_read(uint8_t reg)
{
	io_out8(0x70, reg);
	return io_in8(0x71);
}

static uint8_t rtc_bcd_to_binary(uint8_t value)
{
	return (uint8_t)((value & 0x0f) + ((value >> 4) * 10));
}
#endif

int clock_read_rtc(struct clock_datetime *datetime)
{
#if defined(ARCH_x86_64)
	uint8_t status_b;
	uint8_t century;
	if (!datetime)
		return -1;
	for (uint32_t attempt = 0; attempt < 100000; ++attempt)
		if (!(rtc_read(0x0a) & 0x80))
			break;
	status_b = rtc_read(0x0b);
	datetime->second = rtc_read(0x00);
	datetime->minute = rtc_read(0x02);
	datetime->hour = rtc_read(0x04);
	datetime->day = rtc_read(0x07);
	datetime->month = rtc_read(0x08);
	century = rtc_read(0x32);
	datetime->year = (uint16_t)(century * 100 + rtc_read(0x09));
	if (!(status_b & 0x04)) {
		datetime->second = rtc_bcd_to_binary(datetime->second);
		datetime->minute = rtc_bcd_to_binary(datetime->minute);
		datetime->hour = rtc_bcd_to_binary(datetime->hour);
		datetime->day = rtc_bcd_to_binary(datetime->day);
		datetime->month = rtc_bcd_to_binary(datetime->month);
		datetime->year = (uint16_t)(rtc_bcd_to_binary(century) * 100 +
		                             rtc_bcd_to_binary(rtc_read(0x09)));
	}
	return datetime->year >= 1970 && datetime->month >= 1 &&
	       datetime->month <= 12 && datetime->day >= 1 &&
	       datetime->day <= 31 ? 0 : -1;
#else
	(void)datetime;
	return -1;
#endif
}
