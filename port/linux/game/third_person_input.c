/* Live distance adjustment uses the same bounded setting as the menu. */
#include "cseries.h"
#include "third_person.h"

double config_real(char const *name);
int config_write(char const *name, char const *value);

int third_person_distance_scroll(float notches)
{
	double distance;
	char value[32];
	if (!third_person_active(0) || !third_person_available(0) ||
		!(notches >= -100.0f && notches <= 100.0f) || notches == 0.0f)
		return 0;
	distance = config_real("display.third_person_distance");
	if (!(distance >= 0.3 && distance <= 2.0))
		distance = 0.85;
	distance = PIN(distance - (double)notches * 0.08, 0.3, 2.0);
	snprintf(value, sizeof(value), "%.4f", distance);
	config_write("display.third_person_distance", value);
	return 1;
}
