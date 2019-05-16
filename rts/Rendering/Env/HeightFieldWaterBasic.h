/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef HEIGHT_FIELD_WATER_BASIC_H
#define HEIGHT_FIELD_WATER_BASIC_H

#include "IWater.h"

typedef struct {
    bool processed;         // If this tile was solved at this or higher level
    bool drawn;             // If this tile must be drawn
    unsigned char level;    // Level at which this tile was drawn
    float height;
} waterLODStruct;

// NOTE: a tile processed but not drawn is a tile with ground

typedef struct {
    int x,y,stride;
} waterLODSizeStruct;

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
        
        void drawTile(float h_x_y, float h_x1_y, float h_x_y1, float h_x1_y1, 
            int x, int y, int dx, int dy, float repeatX, float repeatY);
        
	unsigned int textureID;
	unsigned int displistID;
        
        unsigned int tx,ty;
        
        const float mapSizeX;
	const float mapSizeY;
        
        const float threshold;
        
        const float alpha;
        
        int levels;
        
        waterLODSizeStruct *waterLODSize;
        waterLODStruct **waterLOD;
};

#endif // HEIGHT_FIELD_WATER_BASIC_H
