/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "HeightFieldWaterUpdaterBasic.h"
#include "ReadMap.h"

CHeightFieldWaterUpdaterBasic::CHeightFieldWaterUpdaterBasic(CReadMap* map):IHeightFieldWaterUpdater(map) {
}

// Returns new water height in a cell, based on the difference with its neighbor
float flow_adjust(float diff, float r)
{
    float r_new;
    if(diff > 0) {
        // The other cell is higher, water flows to the current cell
        if(diff<r) {
            r_new = (r - diff)/2+diff;
        } else {
            // All water flows to the current cell
            r_new = r;
        }
    } else {
        // The other cell is lower, water flows out of the current cell
        if(-diff<r) {
            r_new = (r + diff)/2;
        } else {
            // All water flows from the current cell
            r_new = 0;
        }
    }
    return r_new;
}

void CHeightFieldWaterUpdaterBasic::UpdateStep() {
    // For reference see:
    // http://matthias-mueller-fischer.ch/talks/GDC2008.pdf
    // http://isg.cs.tcd.ie/cosulliv/Pubs/LeeShallowEqs.pdf
    const float* const centerHeightMap = map->GetCenterHeightMapSynced();
  
    float* heightWaterMap = this->GetHeightWaterMap();
    
    // How much water flows from cell x,y to cell x-1, y
    float* waterMapFlowX = this->GetWaterMapFlowX(); 
    
    // How much water flows from cell x,y to cell x, y-1
    float* waterMapFlowY = this->GetWaterMapFlowY();
    
    float volumeInitial = 0, volumeNew = 0;
    
    const float THRESHOLD = 0.2f;
    const float ATTENUATION = 0.99f;

    for (int x = 0; x < mapDims.mapx; x++) {
        for (int y = 0; y < mapDims.mapy; y++) {
            volumeInitial += heightWaterMap[(y) * mapDims.mapx + x];
        }
    }
    
    // If the map is dry don't perform any computation
    if(volumeInitial < THRESHOLD) {
        return;
    }
    
    // We make an "approximation" - the edges of the map are considered to be 
    // an infinite height wall. This means there will be no flow from/to those
    // locations, and we do not update/compute their water height.

    // Compute the flows in cell x,y based on the water and terrain height of
    // neighbor cells. 
    for (int x = 1; x < mapDims.mapxm1; x++) {
        for (int y = 1; y < mapDims.mapym1; y++) {

            float rho;      // Total water in current and neighbor cell
            float rho_new;  // New water height in current cell
            float diff;     // Height difference between neighbor cell and current 

            // Adjust the flow on X axis.
            rho = heightWaterMap[x - 1 + mapDims.mapx * y] + heightWaterMap[x + mapDims.mapx * y];
            diff = centerHeightMap[x - 1 + mapDims.mapx * y] - centerHeightMap[x + mapDims.mapx * y];
            if (x == 1) {
                // We consider the map has an infinite wall at x = 0, so we bypass the actual values
                rho = heightWaterMap[x + mapDims.mapx * y];
                diff = std::numeric_limits<float>::infinity();
            }
            rho_new = flow_adjust (diff, rho);

            // Based on the difference between how much water the cell should have
            // and what it has, we adjust the flow
            waterMapFlowX[x + mapDims.mapx * y] += (rho_new - heightWaterMap[x + mapDims.mapx * y]);
            waterMapFlowX[x + mapDims.mapx * y] *= ATTENUATION;

            // Adjust the flow on Y axis
            rho = heightWaterMap[x + mapDims.mapx * (y - 1)] + heightWaterMap[x + mapDims.mapx * y];
            diff = centerHeightMap[x + mapDims.mapx * (y - 1)] - centerHeightMap[x + mapDims.mapx * y];
            if (y == 1) {
                // We consider the map has an infinite wall at y = 0, so we bypass the actual values
                rho = heightWaterMap[x + mapDims.mapx * y];
                diff =  std::numeric_limits<float>::infinity();
            }
            rho_new = flow_adjust (diff, rho);

            // Based on the difference between how much water the cell should have
            // and what it has, we adjust the flow
            waterMapFlowY[x + mapDims.mapx * y] += (rho_new - heightWaterMap[x + mapDims.mapx * y]);
            waterMapFlowY[x + mapDims.mapx * y] *= ATTENUATION;
        }
    }

    for (int x = 1; x < mapDims.mapxm1; x++) {
        for (int y = 1; y < mapDims.mapym1; y++) {
            // Compute the flow into this cell
            float flow = waterMapFlowX[x + 1 + mapDims.mapx * y] + waterMapFlowY[x + mapDims.mapx * (y + 1)] - waterMapFlowX[x + mapDims.mapx * y] - waterMapFlowY[x + mapDims.mapx * y];
            // Adjust the water height
            heightWaterMap[x + mapDims.mapx * y] -= flow;
            if (heightWaterMap[x + mapDims.mapx * y] < THRESHOLD) {
                heightWaterMap[x + mapDims.mapx * y] = 0;
            }
        }
    }
    int cnt = 0;
    float rho_adjust;
    do {
        // Number of squares that have some water, to redistribute any water lost
        // due to numerical approximations
        int nonZero = 0;
        volumeNew = 0;
        for (int x = 0; x < mapDims.mapx; x++) {
            for (int y = 0; y < mapDims.mapy; y++) {
                volumeNew += heightWaterMap[(y) * mapDims.mapx + x];
                heightWaterMap[(y) * mapDims.mapx + x] > THRESHOLD && nonZero++;
            }
        }
        rho_adjust = volumeInitial - volumeNew;
        for (int x = 1; x < mapDims.mapxm1; x++) {
            for (int y = 1; y < mapDims.mapym1; y++) {
                if (heightWaterMap[(y) * mapDims.mapx + x] > THRESHOLD) {
                    heightWaterMap[x + mapDims.mapx * y] += rho_adjust / nonZero;
                }
                if (heightWaterMap[x + mapDims.mapx * y] < 0) {
                    heightWaterMap[x + mapDims.mapx * y] = 0;
                }
            }
        }
        cnt++;
    }
    while (fabs (rho_adjust / volumeInitial) > 0.01 && cnt<10);
    
    if(cnt==10) {
        printf("ERROR: water volume redistribution failing!");
    }
    
    // For drawing purposes, we put the water on the edges same as their immediate
    // neighbor
    for (int x = 0; x < mapDims.mapx; x++) {
        heightWaterMap[x + mapDims.mapx * 0] = heightWaterMap[x + mapDims.mapx * 1];
        heightWaterMap[x + mapDims.mapx * (mapDims.mapy-1)] = heightWaterMap[x + mapDims.mapx * (mapDims.mapy-2)];
    }
    
    for (int y = 0; y < mapDims.mapy; y++) {
        heightWaterMap[0 + mapDims.mapx * y] = heightWaterMap[1 + mapDims.mapx * y];
        heightWaterMap[mapDims.mapx-1 + mapDims.mapx * y] = heightWaterMap[mapDims.mapx-2 + mapDims.mapx * y];
    }
}

