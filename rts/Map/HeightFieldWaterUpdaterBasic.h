/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef HEIGHT_FIELD_WATER_UPDATER_BASIC_H
#define HEIGHT_FIELD_WATER_UPDATER_BASIC_H

#include "Map/HeightFieldWaterUpdater.h"

class CHeightFieldWaterUpdaterBasic : public IHeightFieldWaterUpdater
{
public:
        CHeightFieldWaterUpdaterBasic(CReadMap* map);
	~CHeightFieldWaterUpdaterBasic() {};

	void UpdateStep();
};

#endif // HEIGHT_FIELD_WATER_UPDATER_H
