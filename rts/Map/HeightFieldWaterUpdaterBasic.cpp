/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "HeightFieldWaterUpdaterBasic.h"
#include "ReadMap.h"

CHeightFieldWaterUpdaterBasic::CHeightFieldWaterUpdaterBasic(CReadMap* map):IHeightFieldWaterUpdater(map) {
}

float flow_adjust(float diff, float r)
{
    float r_new;
    if(diff > 0) {
        if(diff<r) {
            
            r_new = (r - diff)/2+diff;
        } else {
            // All water flows to the current cell
            r_new = r;
        }
    } else {
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
    float* waterMapFlowX = this->GetWaterMapFlowX();
    float* waterMapFlowY = this->GetWaterMapFlowY();

    float volumeInitial = 0, volumeNew = 0;
    
    const float THRESHOLD = 0.1f;

    for (int x = 0; x < mapDims.mapx; x++) {
            for (int y = 0; y < mapDims.mapy; y++) {
                    volumeInitial += heightWaterMap[(y) * mapDims.mapx + x];
            }
    }
    
    // If the map is dry don't perform any computation
    if(volumeInitial < THRESHOLD) {
        return;
    }

#define ATTENUATION 0.98
#define LIMIT_EXECS 3

    // Compute the flows in cell x,y based on the water and terrain height of
    // neighbor cells
    for (int x = 1; x < mapDims.mapxm1; x++) {
        for (int y = 1; y < mapDims.mapym1; y++) {

            float rho;      // Water density
            float rho_new;  // New water density
            float diff;     // Height difference between neighbor cell and current 

            // Adjust the flow on X axis
            rho = heightWaterMap[x - 1 + mapDims.mapx * y] + heightWaterMap[x + mapDims.mapx * y];
            diff = centerHeightMap[x - 1 + mapDims.mapx * y] - centerHeightMap[x + mapDims.mapx * y];
            
            if (x == 1) {
                rho = heightWaterMap[x + mapDims.mapx * y];
                diff = std::numeric_limits<float>::infinity();
            }
            rho_new = flow_adjust (diff, rho);

            waterMapFlowX[x + mapDims.mapx * y] += (rho_new - heightWaterMap[x + mapDims.mapx * y]);
            waterMapFlowX[x + mapDims.mapx * y] *= ATTENUATION;

            // Adjust the flow on Y axis
            rho = heightWaterMap[x + mapDims.mapx * (y - 1)] + heightWaterMap[x + mapDims.mapx * y];
            diff = centerHeightMap[x + mapDims.mapx * (y - 1)] - centerHeightMap[x + mapDims.mapx * y];
// TODO: fix somehow else
            if (y == 1) {
                    diff = 1000000;
                    rho = heightWaterMap[x + mapDims.mapx * y];
            }
            rho_new = flow_adjust (diff, rho);

            waterMapFlowY[x + mapDims.mapx * y] += (rho_new - heightWaterMap[x + mapDims.mapx * y]);
            waterMapFlowY[x + mapDims.mapx * y] *= ATTENUATION;
        }
    }

    // Perform flow adjustment. There is no 
    unsigned execs = 0, change;
    do {
        execs++;
        change = 0;
        for (int x = 1; x < mapDims.mapxm1; x++) {
            for (int y = 1; y < mapDims.mapym1; y++) {
                float fl = waterMapFlowX[x + 1 + mapDims.mapx * y] + waterMapFlowY[x + mapDims.mapx * (y + 1)] - waterMapFlowX[x + mapDims.mapx * y] - waterMapFlowY[x + mapDims.mapx * y];
                if (fl > THRESHOLD && (fl - heightWaterMap[x + mapDims.mapx * y]) > THRESHOLD) {
                    change = 1;
///printf(" flow is %8.3f rho is %8.3f \n",fl,heightWaterMap[x+mapDims.mapx*y]);
                    if (heightWaterMap[x + mapDims.mapx * y] > THRESHOLD) {
                        float r = fl / heightWaterMap[x + mapDims.mapx * y];

                        waterMapFlowX[x + 1 + mapDims.mapx * y] /= r;
                        waterMapFlowY[x + mapDims.mapx * (y + 1)] /= r;

                        waterMapFlowX[x + mapDims.mapx * y] /= r;
                        waterMapFlowY[x + mapDims.mapx * y] /= r;
                    }
                    else {
                        waterMapFlowX[x + 1 + mapDims.mapx * y] = 0;
                        waterMapFlowY[x + mapDims.mapx * (y + 1)] = 0;

                        waterMapFlowX[x + mapDims.mapx * y] = 0;
                        waterMapFlowY[x + mapDims.mapx * y] = 0;
                    }
                }
            }
        }
        //       printf("flow adjustment change = %d\n",change);
    }
    while (change == 1 && execs < LIMIT_EXECS);

    for (int x = 1; x < mapDims.mapxm1; x++) {
        for (int y = 1; y < mapDims.mapym1; y++) {
            float fl = waterMapFlowX[x + 1 + mapDims.mapx * y] + waterMapFlowY[x + mapDims.mapx * (y + 1)] - waterMapFlowX[x + mapDims.mapx * y] - waterMapFlowY[x + mapDims.mapx * y];
            if (fl < heightWaterMap[x + mapDims.mapx * y]) {
                heightWaterMap[x + mapDims.mapx * y] -= fl;
                if (heightWaterMap[x + mapDims.mapx * y] < THRESHOLD) {
                    heightWaterMap[x + mapDims.mapx * y] = 0;
                    waterMapFlowX[x + mapDims.mapx * y] = 0;
                    waterMapFlowY[x + mapDims.mapx * y] = 0;
                }
            }
            else {
                heightWaterMap[x + mapDims.mapx * y] = 0;
            }
        }
    }
    int cnt = 0;
    float rho_adjust;
    do {
        // Number of squares that have some water
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
}

