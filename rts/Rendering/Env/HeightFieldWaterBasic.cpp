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

CHeightFieldWaterBasic::CHeightFieldWaterBasic()
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


unsigned int
CHeightFieldWaterBasic::GenWaterDynamicQuadsList (unsigned int textureWidth, unsigned int textureHeight) const
{
	unsigned int listID = glGenLists (1);

	glNewList (listID, GL_COMPILE);
	glDisable (GL_ALPHA_TEST);
	glDepthMask (0);
	glColor4f (0.7f, 0.7f, 0.7f, 0.5f);
	glEnable (GL_TEXTURE_2D);
	glBindTexture (GL_TEXTURE_2D, textureID);
	glBegin(GL_TRIANGLES);

	const float mapSizeX = mapDims.mapx * SQUARE_SIZE;
	const float mapSizeY = mapDims.mapy * SQUARE_SIZE;

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

#define RATIO 3
	for (int x = 0; x < mapDims.mapx - RATIO * 2; x += (RATIO + 1)) {
		for (int y = 0; y < mapDims.mapy - RATIO * 2; y += (RATIO + 1)) {
			if (!(heightWaterMap[(y - 1) * mapDims.mapx + x - 1] > 0.001 ||
			    heightWaterMap[(y - 1) * mapDims.mapx + x + 1] > 0.001 ||
			    heightWaterMap[(y + 1) * mapDims.mapx + x - 1] > 0.001 ||
			    heightWaterMap[(y + 1) * mapDims.mapx + x + 1] > 0.001)) continue;

			float sumdiff;
			sumdiff = fabs ((heightWaterMap[y * mapDims.mapx + x] + centerHeightMap[y * mapDims.mapx + x])
			   - (heightWaterMap[(y + RATIO + 1) * mapDims.mapx + x] + centerHeightMap[(y + RATIO + 1) * mapDims.mapx + x])) / 30;
			glColor4f (0.7f * sumdiff, 0.7f * sumdiff, 0.7f * sumdiff, 0.5f);

			glTexCoord2f (x * repeatX, y * repeatY);
			glVertex3f (x * mapSizeX / mapDims.mapx, heightWaterMap[y * mapDims.mapx + x] + centerHeightMap[y * mapDims.mapx + x], y * mapSizeY / mapDims.mapy);

			glTexCoord2f (x * repeatX, (y + 1) * repeatY);
			glVertex3f (x * mapSizeX / mapDims.mapx, heightWaterMap[(y + RATIO + 1) * mapDims.mapx + x] + centerHeightMap[(y + RATIO + 1) * mapDims.mapx + x], (y + RATIO + 1) * mapSizeY / mapDims.mapy);

//new
			glTexCoord2f ((x + 1) * repeatX, y * repeatY);
			glVertex3f ((x + RATIO + 1) * mapSizeX / mapDims.mapx, heightWaterMap[y * mapDims.mapx + x + RATIO + 1] + centerHeightMap[y * mapDims.mapx + x + RATIO + 1], y * mapSizeY / mapDims.mapy);

///-----
			sumdiff = fabs ((heightWaterMap[(y + RATIO + 1) * mapDims.mapx + x + RATIO + 1] + centerHeightMap[(y + RATIO + 1) * mapDims.mapx + x + RATIO + 1]) - (heightWaterMap[y * mapDims.mapx + x + RATIO + 1] + centerHeightMap[y * mapDims.mapx + x + RATIO + 1])) / 30;
			glColor4f (0.7f * sumdiff, 0.7f * sumdiff, 0.7f * sumdiff, 0.5f);

			glTexCoord2f ((x + 1) * repeatX, (y + 1) * repeatY);
			glVertex3f ((x + RATIO + 1) * mapSizeX / mapDims.mapx, heightWaterMap[(y + RATIO + 1) * mapDims.mapx + x + RATIO + 1] + centerHeightMap[(y + RATIO + 1) * mapDims.mapx + x + RATIO + 1], (y + RATIO + 1) * mapSizeY / mapDims.mapy);

			glTexCoord2f ((x + 1) * repeatX, y * repeatY);
			glVertex3f ((x + RATIO + 1) * mapSizeX / mapDims.mapx, heightWaterMap[y * mapDims.mapx + x + RATIO + 1] + centerHeightMap[y * mapDims.mapx + x + RATIO + 1], y * mapSizeY / mapDims.mapy);

//new
			glTexCoord2f (x * repeatX, (y + 1) * repeatY);
			glVertex3f (x * mapSizeX / mapDims.mapx, heightWaterMap[(y + RATIO + 1) * mapDims.mapx + x] + centerHeightMap[(y + RATIO + 1) * mapDims.mapx + x], (y + RATIO + 1) * mapSizeY / mapDims.mapy);

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

	static int crt=0;
	crt++;
	//if(crt%3==0)
	{
		crt=0;
		glDeleteLists(displistID, 1);
		displistID = GenWaterDynamicQuadsList(tx,ty);
	}

	glCallList(displistID);
	glPopAttrib();
}
