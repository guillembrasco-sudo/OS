#include <drivers/driver.h>

#define MAX_DRIVERS 64

static struct driver *registered_drivers[MAX_DRIVERS];
static unsigned registered_count;

int driver_register(struct driver *driver)
{
	if (driver == 0 || registered_count == MAX_DRIVERS)
		return -1;
	registered_drivers[registered_count++] = driver;
	return 0;
}

int driver_bind(struct device *device)
{
	if (device == 0)
		return -1;
	for (unsigned index = 0; index < registered_count; ++index) {
		struct driver *driver = registered_drivers[index];
		if (driver->probe != 0 && driver->probe(device) != 0)
			continue;
		if (driver->bind != 0 && driver->bind(device) != 0)
			continue;
		device->driver = driver;
		return 0;
	}
	return -1;
}

void driver_unregister(struct driver *driver)
{
	for (unsigned index = 0; index < registered_count; ++index) {
		if (registered_drivers[index] != driver)
			continue;
		for (; index + 1 < registered_count; ++index)
			registered_drivers[index] = registered_drivers[index + 1];
		--registered_count;
		return;
	}
}
