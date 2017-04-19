#include "others.h"

double mx_base_coordinates2double(kal_uint8 *coordinates)
{
	double result = 0;
	char str[14];

	result += (double)coordinates[1];
	result += (double)coordinates[2] * 0.01;
	result += (double)coordinates[3] * 0.0001;
	result += (double)coordinates[4] * 0.000001;
	result /= 60;
	result += (double)coordinates[0];

	memset(str, 0, sizeof(str));
	sprintf(str, "%13f", result);
	MX_BASE_PRINT("%s(%d %d.%02d%02d%02d): %s", __FUNCTION__, coordinates[0], coordinates[1], coordinates[2], coordinates[3], coordinates[4], str);
	return result;
}

static char imei[] = "123456789012345";
static char release[] = "MXT1608S-V100C001B006s";
static char gnss[] = "MXT1608S-V300C001B003E6902";

char * mxapp_get_imei(void)
{
	return imei;
}

char * release_verno(void)
{
	return release;
}

char * gnss_verno(void)
{
	return gnss;
}

kal_uint8 mxapp_battery_get_voltage_percent(void)
{
	return 42;
}