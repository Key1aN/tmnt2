#pragma once


/*
 * Enhanced Graphics: finds the stage's water render polygons and builds the
 * stylized water overlay mesh from them.
 *
 * Water is not flagged on render polygons. Each render material is scored by
 *   - how much of it is horizontal,
 *   - how much of that lies over collision polygons with the "water" or
 *     "dobon" attribute (the same attribute splashes and wading use),
 *   - UV animation and its texture name,
 *   - Include / Exclude lists in TMNT2_graphics.ini.
 * The overlay uses only the chosen render triangles (never invented planes),
 * so collision, movement and splash detection are untouched. Every candidate
 * and the decision are written to TMNT2_graphics.log.
 */
namespace WaterSurface
{
    void Build(RpWorld* pWorld, RpWorld* pWorldCollision, const char* pszStageName);
    void Clear(void);

    /* v1.1.2: writes the current stage's water textures (chosen water and every named /
     * UV-animated candidate) as 32-bit TGA files to TMNT2_water_textures\. Returns the count. */
    int32 ExportTextures(void);
};
