/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "HeightFieldWaterUpdater.h"
#include "ReadMap.h"
#include "Game/GameSetup.h"

IHeightFieldWaterUpdater* mapHeightFieldWaterUpdater;
 
void IHeightFieldWaterUpdater::Update() {
    if(gameSetup->heightFieldWater==0) {
        return;
    };
    this->UpdateStep();
}

float* IHeightFieldWaterUpdater::GetHeightWaterMap() { return &(map->waterMapRho[0]); }
float* IHeightFieldWaterUpdater::GetWaterMapFlowX() { return &(map->waterMapFlowX[0]); }
float* IHeightFieldWaterUpdater::GetWaterMapFlowY() { return &(map->waterMapFlowY[0]); }
