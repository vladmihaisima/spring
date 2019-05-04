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
    threshold(0.01f), alpha(1.0f)
{
	CBitmap waterTexBM;
	if (!waterTexBM.Load(waterRendering->texture)) {
		LOG_L(L_WARNING, "[%s] could not read water texture from file \"%s\"", __FUNCTION__, waterRendering->texture.c_str());

		// fallback
		waterTexBM.AllocDummy(SColor(0,0,255,255));
	}

	// create mipmapped texture
	textureID = waterTexBM.CreateMipMapTexture();
	displistID = GenWaterDynamicQuadsList(waterTexBM.xsize, waterTexBM.ysize);

	tx=waterTexBM.xsize;
	ty=waterTexBM.ysize;
}

CHeightFieldWaterBasic::~CHeightFieldWaterBasic()
{
	glDeleteTextures(1, &textureID);
	glDeleteLists(displistID, 1);
}

/**
 * Draws a quad (2 triangles) of water, using coordinates x,y and x+dx, y+dy
 */
void CHeightFieldWaterBasic::drawTile(const float* centerHeightMap, const float* heightWaterMap, 
        int x, int y, int dx, int dy, float repeatX, float repeatY) {

    float hW_x_y = heightWaterMap[y * mapDims.mapx + x];
    float hW_x1_y = heightWaterMap[y * mapDims.mapx + x + dx];
    float hW_x_y1 = heightWaterMap[(y + dy) * mapDims.mapx + x];
    float hW_x1_y1 = heightWaterMap[(y + dy) * mapDims.mapx + x + dx];
                                
    // Heights of the four corners of the grid that we draw
    float h_x_y = heightWaterMap[y * mapDims.mapx + x] + centerHeightMap[y * mapDims.mapx + x];
    float h_x1_y = heightWaterMap[y * mapDims.mapx + x + dx] + centerHeightMap[y * mapDims.mapx + x + dx];
    float h_x_y1 = heightWaterMap[(y + dy) * mapDims.mapx + x] + centerHeightMap[(y + dy) * mapDims.mapx + x];
    float h_x1_y1 = heightWaterMap[(y + dy) * mapDims.mapx + x + dx] + centerHeightMap[(y + dy) * mapDims.mapx + x + dx];

    h_x_y = (hW_x_y>0.001) ? h_x_y : h_x_y - 1;
    h_x1_y = (hW_x1_y>0.001) ? h_x1_y : h_x1_y - 1;
    h_x_y1 = (hW_x_y1>0.001) ? h_x_y1 : h_x_y1 - 1;
    h_x1_y1 = (hW_x1_y1>0.001) ? h_x1_y1 : h_x1_y1 - 1;
    
    float sumdiff;
        
    // First triangle
    sumdiff = fabs ((heightWaterMap[y * mapDims.mapx + x] + centerHeightMap[y * mapDims.mapx + x])
       - (heightWaterMap[(y + dy + 1) * mapDims.mapx + x] + centerHeightMap[(y + dy + 1) * mapDims.mapx + x])) / 30;
    //glColor4f (0.7f * sumdiff, 0.7f * sumdiff, 0.7f * sumdiff, alpha);
    glColor4f (1,0,0, alpha);

    glTexCoord2f (x * repeatX, y * repeatY);
    glVertex3f (x * mapSizeX / mapDims.mapx, h_x_y, y * mapSizeY / mapDims.mapy);

    glTexCoord2f (x * repeatX, (y + 1) * repeatY);
    glVertex3f (x * mapSizeX / mapDims.mapx, h_x_y1, (y + dy) * mapSizeY / mapDims.mapy);

    glTexCoord2f ((x + 1) * repeatX, y * repeatY);
    glVertex3f ((x + dx) * mapSizeX / mapDims.mapx, h_x1_y, y * mapSizeY / mapDims.mapy);

    // Second triangle
    sumdiff = fabs ((heightWaterMap[(y + dy) * mapDims.mapx + x + dx] + centerHeightMap[(y + dy) * mapDims.mapx + x + dx]) - 
            (heightWaterMap[y * mapDims.mapx + x + dx] + centerHeightMap[y * mapDims.mapx + x + dx])) / 30;
    glColor4f (0.7f * sumdiff, 0.7f * sumdiff, 0.7f * sumdiff, alpha);

    glTexCoord2f ((x + 1) * repeatX, (y + 1) * repeatY);
    glVertex3f ((x + dx) * mapSizeX / mapDims.mapx, h_x1_y1, (y + dy) * mapSizeY / mapDims.mapy);

    glTexCoord2f ((x + 1) * repeatX, y * repeatY);
    glVertex3f ((x + dx) * mapSizeX / mapDims.mapx, h_x1_y, y * mapSizeY / mapDims.mapy);

    glTexCoord2f (x * repeatX, (y + 1) * repeatY);
    glVertex3f (x * mapSizeX / mapDims.mapx, h_x_y1, (y + dy) * mapSizeY / mapDims.mapy);
}

 void CHeightFieldWaterBasic::drawWaterLOD(const float* centerHeightMap, const float* heightWaterMap, 
        int x, int y, int tileX, int tileY, float repeatX, float repeatY) {
    
    // Check if the water height for any of the points inside the tile is close 
    // to ground
    bool closeToGround = false;

    // We check all corners of the tiles, hence the less than equal test
    for (int x1 = x; x1 <= x + tileX; x1++) {
        for (int y1 = y; y1 <= y + tileY; y1++) {
            if(heightWaterMap[y * mapDims.mapx + x] <  threshold) {
                closeToGround = true;
                break;
            }
        }
    }

    if (closeToGround) {
        // Some water height is close to ground, we draw at the highest resolution
        for (int x1 = x; x1 < x + tileX; x1++) {
            for (int y1 = y; y1 < y + tileY; y1++) {
                float hW_x_y = heightWaterMap[y1 * mapDims.mapx + x1];
                float hW_x1_y = heightWaterMap[y1 * mapDims.mapx + x1];
                float hW_x_y1 = heightWaterMap[(y1 + 1) * mapDims.mapx + x1];
                float hW_x1_y1 = heightWaterMap[(y1 + 1) * mapDims.mapx + x1 + 1];

                // In case there is no water, do not draw the tile
                if (!(hW_x_y>threshold || hW_x1_y>threshold || 
                        hW_x_y1>threshold || hW_x1_y1>threshold)) continue;

                drawTile(centerHeightMap, heightWaterMap,
                    x1, y1, 1, 1, repeatX, repeatY);
            }
        }
    } else {
        // No water height is close to ground, we draw only two triangles per tile
        drawTile(centerHeightMap, heightWaterMap,
                x,  y, tileX, tileY, repeatX, repeatY);
    }
}  

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

        // We consider lodGridSize x lodGridSize squares when drawing water that
        // does not intersect land.
        unsigned lodGridSize = 4;

	for (int x = 0; x < mapDims.mapx - lodGridSize; x += lodGridSize) {
            for (int y = 0; y < mapDims.mapy - lodGridSize; y += lodGridSize) {
                drawWaterLOD(centerHeightMap, heightWaterMap,
                    x, y, lodGridSize, lodGridSize, repeatX, repeatY);
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
