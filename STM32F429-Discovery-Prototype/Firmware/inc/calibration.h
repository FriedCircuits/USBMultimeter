#ifndef _CALIBRATION_H
#define _CALIBRATION_H

#include <string.h>
#include "gfx.h"

// Calibration data
float calibrationData[] = {
	-0.06451,		// ax
	7e-05,		// bx
	250.296,		// cy
	0.00047,		// ay
	0.08878,		// by
	-34.3725 		// cy
};

// The loading routine
bool_t LoadMouseCalibration(unsigned instance, void *data, size_t sz)
{
	(void)instance;
	
	if (sz != sizeof(calibrationData)) {
		return FALSE;
	}
	
	memcpy(data, (void*)&calibrationData, sz);
	
	return TRUE;
}

#endif /* _CALIBRATION_H */

