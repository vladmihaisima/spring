/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef HEIGHT_FIELD_WATER_BASIC_H
#define HEIGHT_FIELD_WATER_BASIC_H

#include "IWater.h"

class CHeightFieldWaterBasic : public IWater
{
public:
	CHeightFieldWaterBasic();
	~CHeightFieldWaterBasic();

	void Draw();
	void UpdateWater(CGame* game) {}
	int GetID() const { return HEIGHT_FIELD_WATER_RENDERER_BASIC; }
	const char* GetName() const { return "height field basic"; }

private:
	unsigned int GenWaterDynamicQuadsList(unsigned int textureWidth, unsigned int textureHeight);
        
        void drawTile(const float* centerHeightMap, const float* heightWaterMap, 
            int x, int y, int dx, int dy, float repeatX, float repeatY);
        
        void drawWaterLOD(const float* centerHeightMap, const float* heightWaterMap, 
            int x, int y, int tileX, int tileY, float repeatX, float repeatY);

	unsigned int textureID;
	unsigned int displistID;
        
        unsigned int tx,ty;
        
        const float mapSizeX;
	const float mapSizeY;
        
        const float threshold;
        
        const float alpha;
};

#endif // HEIGHT_FIELD_WATER_BASIC_H
