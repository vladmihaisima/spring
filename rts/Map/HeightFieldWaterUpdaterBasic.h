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
protected:
        // Used to determine path update requirements, a backup version of CReadMap::waterMapRho
	std::vector<float> waterMapRhoPrevious;          //< size: (mapx  )*(mapy  ) (per face)
        
        // Difference between previous and current waterMapRho
        std::vector<float> waterMapRhoDiff;
        
        // Sends the relevant updates for pathing
        void UpdateTerrain();
            
};

#endif // HEIGHT_FIELD_WATER_UPDATER_H
