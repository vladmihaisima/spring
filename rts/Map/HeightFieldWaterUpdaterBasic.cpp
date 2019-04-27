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
          r_new = r;
        }
    } else {
        if(-diff<r) {
          r_new = (r + diff)/2;
        } else {
          r_new = 0;
        }
    }
    return r_new;
}

void CHeightFieldWaterUpdaterBasic::UpdateStep() {

    const float* const centerHeightMap = map->GetCenterHeightMapSynced();
  
    float* heightWaterMap = this->GetHeightWaterMap();
    float* waterMapFlowX = this->GetWaterMapFlowX();
    float* waterMapFlowY = this->GetWaterMapFlowY();

    // Water level update
    for (int rrr = 0; rrr < 1; rrr++) {

            float volInit = 0, volNew = 0;

            // octave script:   vol = sum(rho(:));
            for (int x = 0; x < mapDims.mapx; x++) {
                    for (int y = 0; y < mapDims.mapy; y++) {
                            volInit += heightWaterMap[(y) * mapDims.mapx + x];
                    }
            }

#define ATTENUATION 0.98
#define LIMIT_EXECS 3

            // Below this we consider the value to be 0 (now set to 0.01%)
            //float TRESHOLD = volInit/(mapDims.mapx*mapDims.mapy*100*100);

            float TRESHOLD = 0.1;

            for (int x = 1; x < mapDims.mapxm1; x++) {
                    for (int y = 1; y < mapDims.mapym1; y++) {

                            float r;
                            float r_new;
                            float diff;
                            r = heightWaterMap[x - 1 + mapDims.mapx * y] + heightWaterMap[x + mapDims.mapx * y];
                            diff = centerHeightMap[x - 1 + mapDims.mapx * y] - centerHeightMap[x + mapDims.mapx * y];

// TODO: fix somehow else
                            if (x == 1) {
                                    diff = 1000000;
                                    r = heightWaterMap[x + mapDims.mapx * y];
                            }
                            r_new = flow_adjust (diff, r);

                            waterMapFlowX[x + mapDims.mapx * y] += (r_new - heightWaterMap[x + mapDims.mapx * y]);
                            waterMapFlowX[x + mapDims.mapx * y] *= ATTENUATION;

                            r = heightWaterMap[x + mapDims.mapx * (y - 1)] + heightWaterMap[x + mapDims.mapx * y];
                            diff = centerHeightMap[x + mapDims.mapx * (y - 1)] - centerHeightMap[x + mapDims.mapx * y];
// TODO: fix somehow else
                            if (y == 1) {
                                    diff = 1000000;
                                    r = heightWaterMap[x + mapDims.mapx * y];
                            }
                            r_new = flow_adjust (diff, r);

                            waterMapFlowY[x + mapDims.mapx * y] += (r_new - heightWaterMap[x + mapDims.mapx * y]);
                            waterMapFlowY[x + mapDims.mapx * y] *= ATTENUATION;
                    }
            }

            unsigned execs = 0, change;
            do {
                    execs++;
                    change = 0;
                    for (int x = 1; x < mapDims.mapxm1; x++) {
                            for (int y = 1; y < mapDims.mapym1; y++) {
                                    float fl = waterMapFlowX[x + 1 + mapDims.mapx * y] + waterMapFlowY[x + mapDims.mapx * (y + 1)] - waterMapFlowX[x + mapDims.mapx * y] - waterMapFlowY[x + mapDims.mapx * y];
                                    if (fl > TRESHOLD && (fl - heightWaterMap[x + mapDims.mapx * y]) > TRESHOLD) {
                                            change = 1;
printf(" flow is %8.3f rho is %8.3f \n",fl,heightWaterMap[x+mapDims.mapx*y]);
                                            if (heightWaterMap[x + mapDims.mapx * y] > TRESHOLD) {
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
                                    if (heightWaterMap[x + mapDims.mapx * y] < TRESHOLD) {
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

                    int nz = 0;
                    volNew = 0;
                    for (int x = 0; x < mapDims.mapx; x++) {
                            for (int y = 0; y < mapDims.mapy; y++) {
                                    volNew += heightWaterMap[(y) * mapDims.mapx + x];
                                    if (heightWaterMap[(y) * mapDims.mapx + x] > TRESHOLD)
                                            nz++;
                            }
                    }
                    rho_adjust = volInit - volNew;
                    if (nz == 0)
                            break;
                    for (int x = 1; x < mapDims.mapxm1; x++) {
                            for (int y = 1; y < mapDims.mapym1; y++) {
                                    if (heightWaterMap[(y) * mapDims.mapx + x] > TRESHOLD) {
                                            heightWaterMap[x + mapDims.mapx * y] += rho_adjust / nz;
                                    }
                                    if (heightWaterMap[x + mapDims.mapx * y] < 0) {
                                            heightWaterMap[x + mapDims.mapx * y] = 0;
                                    }
                            }
                    }
                    cnt > 10 && printf ("cnt=%3d volInit is %f, volNew is %f rho adjust is %f nz is %d\n", cnt++, volInit, volNew, rho_adjust, nz);
            }
            while (fabs (rho_adjust / volInit) > 0.01);

    }
    
}

