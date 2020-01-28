/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */


#include "HeightFieldWaterBasic.h"
#include "ISky.h"
#include "WaterRendering.h"

#include "Rendering/GL/myGL.h"
#include "Rendering/Textures/Bitmap.h"
#include "Map/MapInfo.h"
#include "Map/ReadMap.h"
#include "System/Log/ILog.h"
#include "System/TimeProfiler.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CHeightFieldWaterBasic::CHeightFieldWaterBasic() : 
    mapSizeX(mapDims.mapx * SQUARE_SIZE), 
    mapSizeY(mapDims.mapy * SQUARE_SIZE),
    threshold(0.1f), thresholdDiff(10.0f), alpha(0.6f) 
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
            waterLOD[i] = new waterLODStruct[(x+1)*(y+1)]();
            // We add one such that we have some padding to the tiles on the map edges
            waterLODSize[i].x = x+1;
            waterLODSize[i].y = y+1;
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

float CHeightFieldWaterBasic::getMapHeight(int level, int x, int y) {
    int xM = x * waterLODSize[level].stride;   // x in Map coordinates
    int yM = y * waterLODSize[level].stride;   // y in Map coordinates
    return readMap->GetCenterHeightMapSynced()[yM * mapDims.mapx + xM];
}

void printWaterLOD(waterLODStruct* w) {
    printf("drawn %d processed %d level %d height %f\n",w->drawn,w->processed,w->level,w->height);
}
  
/** 
 * For the tile with coordinates x,y and level, compute heights on the direction
 * (dx,dy) taking into account neighboring tile. This is done for one level
 * lower.
 */
void CHeightFieldWaterBasic::computeEdge(int level, int x, int y, int dx, int dy, bool compute) {   

    if(x+dx<0 || y+dy<0 || x+dx>=waterLODSize[level].x || y+dy>=waterLODSize[level].y) {
        return;
    }

    if(get(level,x+dx,y+dy).processed && get(level,x,y).processed) {
        // Both current and neighbor tile are processed, so any height update
        // of lower levels does not influence the display
        return;
    };

    // If tile is drawn at current level (or higher) and 
    // neighbor tile is drawn at a lower level (higher detail), 
    // compute the height on the edge between tiles
    if(get(level,x,y).level>=level && get(level,x+dx,y+dy).level<level) {
        float h = 0;
        if(dx==-1) {
            h+= get(level,x  ,y  ).height;
            h+= get(level,x  ,y+1).height;
            get(level-1,x*2  ,y*2  ).height = get(level,x  ,y  ).height;
            get(level-1,x*2  ,y*2+1).height = h/2;
            get(level-1,x*2  ,y*2+2).height = get(level,x  ,y+1).height;
        }
        if(dx== 1) {
            h+= get(level,x+1,y  ).height;
            h+= get(level,x+1,y+1).height;
            get(level-1,x*2+2,y*2  ).height = get(level,x+1,y  ).height;
            get(level-1,x*2+2,y*2+1).height = h/2;
            get(level-1,x*2+2,y*2+2).height = get(level,x+1,y+1).height;
        }
        if(dy==-1) {
            h+= get(level,x  ,y  ).height;
            h+= get(level,x+1,y  ).height;
            get(level-1,x*2  ,y*2  ).height = get(level,x  ,y  ).height;
            get(level-1,x*2+1,y*2  ).height = h/2;
            get(level-1,x*2+2,y*2  ).height = get(level,x+1,y  ).height;
        }   
        if(dy== 1) {
            h+= get(level,x  ,y+1).height;
            h+= get(level,x+1,y+1).height;
            get(level-1,x*2  ,y*2+2).height = get(level,x  ,y+1).height;
            get(level-1,x*2+1,y*2+2).height = h/2;
            get(level-1,x*2+2,y*2+2).height = get(level,x+1,y+1).height;
        }
    }
}

waterLODStruct& CHeightFieldWaterBasic::get(int level, int x, int y) {
    return waterLOD[level][y*waterLODSize[level].x + x];
}

void CHeightFieldWaterBasic::heightInit(const float* centerHeightMap, const float* heightWaterMap, int level, int xL, int yL, int xM, int yM, float& max) {
    get(level,xL  ,yL  ).height = centerHeightMap[yM  * mapDims.mapx + xM ] + heightWaterMap[yM  * mapDims.mapx + xM ];
    if(heightWaterMap[yM  * mapDims.mapx + xM ]>threshold && max<get(level,xL  ,yL  ).height) max = get(level,xL  ,yL  ).height;
}

unsigned int
CHeightFieldWaterBasic::GenWaterDynamicQuadsList (unsigned int textureWidth, unsigned int textureHeight)
{
    SCOPED_TIMER("Render::CHeightFieldWaterBasic::GenWaterDynamicQuadsList");
        
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
    
    // Reinitialize the structures
    for (i=levels-1;i>=0;i--) {
        memset(waterLOD[i], 0, sizeof(waterLODStruct)*waterLODSize[i].x*waterLODSize[i].y);
    }

    // First pass, determine at which lod levels are relevant (have water and
    // have differences big enough to be worth drawing)
    for (i=levels-1;i>=0;i--) {
        for (int xL = 0; xL < waterLODSize[i].x-1; xL++) {
            for (int yL = 0; yL < waterLODSize[i].y-1; yL++) {
                // Check if the upper level is drawn, then we "skip" checking this
                if(i!=(levels-1) && get(i+1,xL/2,yL/2).processed==true) {
                    get(i,xL,yL).processed = true;
                    get(i,xL,yL).drawn = false;
                    get(i,xL,yL).level = get(i+1,xL/2,yL/2).level;
                    continue;
                }
                
                int xM = xL * waterLODSize[i].stride;   // x in Map coordinates
                int yM = yL * waterLODSize[i].stride;   // y in Map coordinates
                
                // Determine if in this tile there is water
                bool above = false;
                for (int x = xM; x <= xM + waterLODSize[i].stride && !above; x++) {
                    for (int y = yM; y <= yM + waterLODSize[i].stride && !above; y++) {
                        if(heightWaterMap[y * mapDims.mapx + x]>threshold) {
                            above = true;
                        } 
                    }
                }   
                if(!above) {
                    // None or few water
                    get(i,xL,yL).processed = true;
                    get(i,xL,yL).drawn = false;
                    continue;
                }

                // Determine if there is few variation over the tile, which allows
                // us to draw at this lod level. Compute the min and max in the tile
                float minH = centerHeightMap[yM * mapDims.mapx + xM] + heightWaterMap[yM * mapDims.mapx + xM];
                float maxH = centerHeightMap[yM * mapDims.mapx + xM] + heightWaterMap[yM * mapDims.mapx + xM];
                // For large tiles with small variation, we want to check that enough
                // points have actual water (think lowest granularity and one corner
                // with some water, we do not want to draw whole tile as 'filled')
                int belowThreshold = 0;
                float dist = maxH-minH;
                for (int x = xM; x <= xM + waterLODSize[i].stride && dist<thresholdDiff; x++) {
                    for (int y = yM; y <= yM + waterLODSize[i].stride && dist<thresholdDiff; y++) {
                        minH = std::min(centerHeightMap[y * mapDims.mapx + x] + heightWaterMap[y * mapDims.mapx + x], minH); 
                        maxH = std::max(centerHeightMap[y * mapDims.mapx + x] + heightWaterMap[y * mapDims.mapx + x], maxH); 
                        dist = maxH-minH;
                        if(heightWaterMap[y * mapDims.mapx + x]<threshold) belowThreshold++;
                    }
                }

                if((dist < thresholdDiff && belowThreshold<waterLODSize[i].stride*waterLODSize[i].stride/8)|| i==0) {
                    // Small variations in water level and a lot of points with 
                    // actual water, or finest level of lod, we draw
                    get(i,xL,yL).processed = true;
                    get(i,xL,yL).drawn = true;
                    get(i,xL,yL).level = i;
                    continue;
                } else {
                    // Large variations in water level do not process at this level
                    get(i,xL,yL).processed = false;
                    get(i,xL,yL).drawn = false;
                }
            }
        }
    } 

    // TODO: decide to draw tiles neighboring shore tiles (to close gaps due
    // to map adjustments of mesh)
    // We want to draw tiles that are bordering the ground. Can happen that one
    // of the corners is with water, but below threshold, then tile would not get
    // drawn but gap could between ground and other tiles
    
    // Initialize all the heights at the level at which they are drawn
    for (i=levels-1;i>=0;i--) {
        for (int xL = 0; xL < waterLODSize[i].x-1; xL++) {
            for (int yL = 0; yL < waterLODSize[i].y-1; yL++) {
                // We need to compute heights only for the drawn levels
                if(get(i,xL,yL).drawn==false) {
                    continue;
                }
                
                int xM = xL * waterLODSize[i].stride;   // x in Map coordinates
                int yM = yL * waterLODSize[i].stride;   // y in Map coordinates
                int xM1 = (xL+1) * waterLODSize[i].stride;   // x in Map coordinates
                int yM1 = (yL+1) * waterLODSize[i].stride;   // y in Map coordinates  
                
                // For the case in which at least one of the corners of the tile does not have water, but the land
                // height is higher than the sum of water and land heights in the other corners (think hill surrounded
                // by water), the water would "stick" upwards on the slope. We tone the effect down,
                // by making the height the maximum of the others.
                
                float max = std::numeric_limits<float>::min();
                
                heightInit(centerHeightMap, heightWaterMap, i, xL  , yL  , xM , yM , max);
                heightInit(centerHeightMap, heightWaterMap, i, xL+1, yL  , xM1, yM , max);
                heightInit(centerHeightMap, heightWaterMap, i, xL  , yL+1, xM , yM1, max);
                heightInit(centerHeightMap, heightWaterMap, i, xL+1, yL+1, xM1, yM1, max);
                
                if(heightWaterMap[yM  * mapDims.mapx + xM ]<threshold) get(i,xL  ,yL  ).height = max;
                if(heightWaterMap[yM  * mapDims.mapx + xM1]<threshold) get(i,xL+1,yL  ).height = max;
                if(heightWaterMap[yM1 * mapDims.mapx + xM ]<threshold) get(i,xL  ,yL+1).height = max;
                if(heightWaterMap[yM1 * mapDims.mapx + xM1]<threshold) get(i,xL+1,yL+1).height = max;
            }
        }
    }
        
    // Compute the heights on the edges of each tile. Do it top down, and adjust
    // the finer levels in case the neighbor cell is coarser and hence impose
    // a certain height.
    for (i=levels-1;i>=1;i--) {
        for (int xL = 0; xL < waterLODSize[i].x-1; xL++) {
            for (int yL = 0; yL < waterLODSize[i].y-1; yL++) {
                computeEdge(i,xL,yL,-1, 0, false);
                computeEdge(i,xL,yL, 1, 0, false);
                computeEdge(i,xL,yL, 0,-1, false);
                computeEdge(i,xL,yL, 0, 1, false);            
            }
        }
    }
      
    for (i=levels-1;i>=0;i--) {
        for (int xL = 0; xL < waterLODSize[i].x-1; xL++) {
            for (int yL = 0; yL < waterLODSize[i].y-1; yL++) {
                if(get(i,xL,yL).drawn==false) {
                    continue;
                }
                
                int xM = xL * waterLODSize[i].stride;   // x in Map coordinates
                int yM = yL * waterLODSize[i].stride;   // y in Map coordinates
                
                // Heights of the four corners of the grid that we draw
                float h_x_y   = get(i,xL  ,yL  ).height;
                float h_x1_y  = get(i,xL+1,yL  ).height;
                float h_x_y1  = get(i,xL  ,yL+1).height;
                float h_x1_y1 = get(i,xL+1,yL+1).height;

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
    glPolygonMode(GL_FRONT, GL_LINE * wireFrameMode + GL_FILL * (1 - wireFrameMode));

    // TODO (vladms): invoke TerrainChange once we think is necessary (i.e. not every frame)
    static int call = 0;
    call ++; call = call % 3;
    
    // TODO (vladms): is this a good threshold?
    if(call==0) {
        glDeleteLists(displistID, 1);
        displistID = GenWaterDynamicQuadsList(tx,ty);
    }
    
    glCallList(displistID);
    glPopAttrib();
}