/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */


#include "HeightFieldWaterBasic.h"
#include "ISky.h"
#include "WaterRendering.h"

#include "Rendering/GL/myGL.h"
#include "Rendering/Textures/Bitmap.h"
#include "Map/MapInfo.h"
#include "Map/ReadMap.h"
#include "System/Log/ILog.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CHeightFieldWaterBasic::CHeightFieldWaterBasic() : 
    mapSizeX(mapDims.mapx * SQUARE_SIZE), 
    mapSizeY(mapDims.mapy * SQUARE_SIZE),
    threshold(0.1f), alpha(1.0f)
{
	CBitmap waterTexBM;
	if (!waterTexBM.Load(waterRendering->texture)) {
		LOG_L(L_WARNING, "[%s] could not read water texture from file \"%s\"", __FUNCTION__, waterRendering->texture.c_str());

		// fallback
		waterTexBM.AllocDummy(SColor(0,0,255,255));
	}

	tx=waterTexBM.xsize;
	ty=waterTexBM.ysize;
        
        int i;
        int stride = 1;
        levels = 8;
        waterLOD = new waterLODStruct*[levels];
        waterLODSize = new waterLODSizeStruct[levels];
        int x = mapDims.mapx, y = mapDims.mapy;
        for(i=0;i<levels;i++) {
            waterLOD[i] = new waterLODStruct[x*y]();
            waterLODSize[i].x = x;
            waterLODSize[i].y = y;
            waterLODSize[i].stride = stride;
            if(x<4||y<4) {
                break;
            } else {
                x = x/2;
                y = y/2;
                stride = stride*2;
            }
        }
        levels = i;
        
        // create mipmapped texture
	textureID = waterTexBM.CreateMipMapTexture();
	displistID = GenWaterDynamicQuadsList(waterTexBM.xsize, waterTexBM.ysize);
}

CHeightFieldWaterBasic::~CHeightFieldWaterBasic()
{
	glDeleteTextures(1, &textureID);
	glDeleteLists(displistID, 1);
}

int debug_clr = 0;
/**
 * Draws a quad (2 triangles) of water, using coordinates x,y and x+dx, y+dy
 */
void CHeightFieldWaterBasic::drawTile(float h_x_y, float h_x1_y, float h_x_y1, float h_x1_y1, 
        int x, int y, int dxM, int dyM, float repeatX, float repeatY) {

    glColor4f (1,0,debug_clr, alpha);

    glTexCoord2f (x * repeatX, y * repeatY);
    glVertex3f (x * mapSizeX / mapDims.mapx, h_x_y, y * mapSizeY / mapDims.mapy);

    glTexCoord2f (x * repeatX, (y + 1) * repeatY);
    glVertex3f (x * mapSizeX / mapDims.mapx, h_x_y1, (y + dyM) * mapSizeY / mapDims.mapy);

    glTexCoord2f ((x + 1) * repeatX, y * repeatY);
    glVertex3f ((x + dxM) * mapSizeX / mapDims.mapx, h_x1_y, y * mapSizeY / mapDims.mapy);
    
    glColor4f (0,1,debug_clr, alpha);

    glTexCoord2f ((x + 1) * repeatX, (y + 1) * repeatY);
    glVertex3f ((x + dxM) * mapSizeX / mapDims.mapx, h_x1_y1, (y + dyM) * mapSizeY / mapDims.mapy);

    glTexCoord2f ((x + 1) * repeatX, y * repeatY);
    glVertex3f ((x + dxM) * mapSizeX / mapDims.mapx, h_x1_y, y * mapSizeY / mapDims.mapy);

    glTexCoord2f (x * repeatX, (y + 1) * repeatY);
    glVertex3f (x * mapSizeX / mapDims.mapx, h_x_y1, (y + dyM) * mapSizeY / mapDims.mapy);
}

//unsigned int
//CHeightFieldWaterBasic::computeEdge() {
//    
//}

unsigned int
CHeightFieldWaterBasic::GenWaterDynamicQuadsList (unsigned int textureWidth, unsigned int textureHeight)
{
    unsigned int listID = glGenLists (1);

    glNewList (listID, GL_COMPILE);
    glDisable (GL_ALPHA_TEST);
    glDepthMask (0);
    glColor4f (0.7f, 0.7f, 0.7f, 0.5f);
    glEnable (GL_TEXTURE_2D);
    glBindTexture (GL_TEXTURE_2D, textureID);
    glBegin(GL_TRIANGLES);

    // Calculate number of times texture should repeat over the map,
    // taking aspect ratio into account.
    float repeatX = 65536.0f / mapDims.mapx;
    float repeatY = 65536.0f / mapDims.mapy * textureWidth / textureHeight;

    // Use better repeat setting of 1 repeat per 4096 mapx/mapy for the new
    // ocean.jpg while retaining backward compatibility with old maps relying
    // on 1 repeat per 1024 mapx/mapy. (changed 16/05/2007)
    if (waterRendering->texture == "bitmaps/ocean.jpg") {
            repeatX /= 4;
            repeatY /= 4;
    }

    repeatX = (waterRendering->repeatX != 0 ? waterRendering->repeatX : repeatX) / 16;
    repeatY = (waterRendering->repeatY != 0 ? waterRendering->repeatY : repeatY) / 16;

    const float* centerHeightMap = readMap->GetCenterHeightMapSynced();
    const float* heightWaterMap = readMap->GetHeightWaterMapSynced();

    int i;
    // First pass, determine at which lod levels are relevant (have water and
    // have differences big enough to be worth drawing)
    for (i=levels-1;i>=0;i--) {
        for (int xL = 0; xL < waterLODSize[i].x-1; xL++) {
            for (int yL = 0; yL < waterLODSize[i].y-1; yL++) {
                // Check if the upper level is drawn, then we "skip" checking this
                if(i!=(levels-1) && waterLOD[i+1][yL/2*waterLODSize[i+1].x + xL/2].processed==true) {
                    waterLOD[i][yL*waterLODSize[i].x + xL].processed = true;
                    waterLOD[i][yL*waterLODSize[i].x + xL].drawn = false;
                    waterLOD[i][yL*waterLODSize[i].x + xL].level = waterLOD[i+1][yL/2*waterLODSize[i+1].x + xL/2].level;
                    continue;
                }
                
                int xM = xL * waterLODSize[i].stride;   // x in Map coordinates
                int yM = yL * waterLODSize[i].stride;   // y in Map coordinates
                
                // Determine if there is a point above ground
                float above = false;
                for (int x = xM; x <= xM + waterLODSize[i].stride && !above; x++) {
                    for (int y = yM; y <= yM + waterLODSize[i].stride && !above; y++) {
                        if(heightWaterMap[y * mapDims.mapx + x]>threshold) {
                            above = true;
                        } 
                    }
                }
                if(!above) {
                    waterLOD[i][yL*waterLODSize[i].x + xL].processed = true;
                    waterLOD[i][yL*waterLODSize[i].x + xL].drawn = false;
                    continue;
                }

                // Determine the min and max in the cell at current lod
                float minH = centerHeightMap[yM * mapDims.mapx + xM] + heightWaterMap[yM * mapDims.mapx + xM];
                float maxH = centerHeightMap[yM * mapDims.mapx + xM] + heightWaterMap[yM * mapDims.mapx + xM];
                float dist = maxH-minH;
                for (int x = xM; x <= xM + waterLODSize[i].stride && dist<10; x++) {
                    for (int y = yM; y <= yM + waterLODSize[i].stride && dist<10; y++) {
                        minH = std::min(centerHeightMap[y * mapDims.mapx + x] + heightWaterMap[y * mapDims.mapx + x], minH); 
                        maxH = std::max(centerHeightMap[y * mapDims.mapx + x] + heightWaterMap[y * mapDims.mapx + x], maxH); 
                        dist = maxH-minH;
                    }
                }
                // TODO (vladms): add some constant instead of 10
                // We draw if small variation or we are at the most fine grained lod
                if(dist < 10 || i==0) {
                    waterLOD[i][yL*waterLODSize[i].x + xL].processed = true;
                    waterLOD[i][yL*waterLODSize[i].x + xL].drawn = true;
                    waterLOD[i][yL*waterLODSize[i].x + xL].level = i;
                    continue;
                } else {
                    waterLOD[i][yL*waterLODSize[i].x + xL].processed = false;
                    waterLOD[i][yL*waterLODSize[i].x + xL].drawn = false;
                }
            }
        }
    } 
    
    // TODO: decide to draw tiles neighboring shore tiles (to close gaps due
    // to map adjustments of mesh)
    
    for (i=levels-1;i>=0;i--) {
        for (int xL = 0; xL < waterLODSize[i].x-1; xL++) {
            for (int yL = 0; yL < waterLODSize[i].y-1; yL++) {
                // We need to compute heights only for the drawn levels
                waterLODStruct* crtLevel = waterLOD[i];
                if(crtLevel[yL*waterLODSize[i].x + xL].drawn==false) {
                    continue;
                }
                
                // For each edge, there are two cases: 
                // A) neighbor tiles is drawn at a lower lod
                // level (need to update the height at that level based on 
                // an interpolation here)
                // B) neighbor tile is not drawn/drawn at largest lod, nothing
                // to do here
                
                // TODO: compute heights based on the level at which tiles were drawn
                int xM = xL * waterLODSize[i].stride;   // x in Map coordinates
                int yM = yL * waterLODSize[i].stride;   // y in Map coordinates
                crtLevel[yL*waterLODSize[i].x + xL].height       = centerHeightMap[yM * mapDims.mapx + xM]       + heightWaterMap[yM * mapDims.mapx + xM];
                crtLevel[yL*waterLODSize[i].x + xL+1].height     = centerHeightMap[yM * mapDims.mapx + xM+1]     + heightWaterMap[yM * mapDims.mapx + xM+1];
                crtLevel[(yL+1)*waterLODSize[i].x + xL].height   = centerHeightMap[(yM+1) * mapDims.mapx + xM]   + heightWaterMap[(yM+1) * mapDims.mapx + xM];
                crtLevel[(yL+1)*waterLODSize[i].x + xL+1].height = centerHeightMap[(yM+1) * mapDims.mapx + xM+1] + heightWaterMap[(yM+1) * mapDims.mapx + xM+1];
                
            }
        }
    }
    
    for (i=levels-1;i>=0;i--) {
        for (int xL = 0; xL < waterLODSize[i].x-1; xL++) {
            for (int yL = 0; yL < waterLODSize[i].y-1; yL++) {
                waterLODStruct* crtLevel = waterLOD[i];
                if(crtLevel[yL*waterLODSize[i].x + xL].drawn==false) {
                    continue;
                }
                
                int xM = xL * waterLODSize[i].stride;   // x in Map coordinates
                int yM = yL * waterLODSize[i].stride;   // y in Map coordinates
                
                // Heights of the four corners of the grid that we draw
                float h_x_y   = crtLevel[yL*waterLODSize[i].x + xL].height;
                float h_x1_y  = crtLevel[yL*waterLODSize[i].x + xL + 1].height;
                float h_x_y1  = crtLevel[(yL+1)*waterLODSize[i].x + xL].height;
                float h_x1_y1 = crtLevel[(yL+1)*waterLODSize[i].x + xL + 1].height;

                drawTile(h_x_y,h_x1_y,h_x_y1,h_x1_y1,xM,yM,waterLODSize[i].stride,waterLODSize[i].stride,repeatX,repeatY);
            }
        }
    }

    glEnd();
    glDisable(GL_TEXTURE_2D);
    glDepthMask(1);
    glEndList();

    return listID;
}

void CHeightFieldWaterBasic::Draw()
{
    if (!waterRendering->forceRendering && !readMap->HasVisibleWater())
            return;

    glPushAttrib(GL_FOG_BIT | GL_POLYGON_BIT);
    sky->SetupFog();
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE * wireFrameMode + GL_FILL * (1 - wireFrameMode));

    glDeleteLists(displistID, 1);
    displistID = GenWaterDynamicQuadsList(tx,ty);

    glCallList(displistID);
    glPopAttrib();
}
