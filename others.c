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

