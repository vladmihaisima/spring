/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef HEIGHT_FIELD_WATER_UPDATER_H
#define HEIGHT_FIELD_WATER_UPDATER_H

#include "ReadMap.h"

class CReadMap;

/** 
 * This interface allows update of the height field water of a map.
 * It takes into account if the global option HeightFieldWater is enabled, and
 * deals with initializing maps that do not have a separate height field map.
 * 
 * It is a friend of CReadMap to access the private fields. This is implemented
 * as an interface as we can envision different implementations of the
 * height field updater depending on hardware (ex: GLSL, multi-threaded, etc.)
 * 
 * The initialization of the heights is performed in CReadMap.
 */
class IHeightFieldWaterUpdater
{
protected:
        CReadMap *map;
public:
        IHeightFieldWaterUpdater(CReadMap *map) { this->map = map; };
	virtual ~IHeightFieldWaterUpdater() {};

        virtual void Update();
	virtual void UpdateStep() = 0;
        
        float* GetHeightWaterMap();
        float *GetWaterMapFlowX();
        float *GetWaterMapFlowY();
};

extern IHeightFieldWaterUpdater* mapHeightFieldWaterUpdater;

#endif // HEIGHT_FIELD_WATER_UPDATER_H
