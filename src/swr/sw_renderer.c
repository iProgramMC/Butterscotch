#include "sw_renderer.h"
#include <stdio.h>

typedef struct
{
	Renderer base;
	
	// Window Properties
	uint16_t width;
	uint16_t height;
	// Framebuffer
	uint32_t* fb;
	uint16_t fbPitch;
}
SWRenderer;

static void SWRenderer_init(Renderer* renderer, DataWin* dataWin)
{
	SWRenderer* swr = (SWRenderer*) renderer;
	
	renderer->dataWin = dataWin;
	
	swr->fb = safeCalloc(swr->width * swr->height, sizeof(uint32_t));
	swr->fbPitch = swr->width;
	
	fprintf(stderr, "SWRenderer initialized.\n");
}
 
static void SWRenderer_destroy(Renderer* renderer)
{
	SWRenderer* swr = (SWRenderer*) renderer;
	
	(void) swr;
	
	fprintf(stderr, "SWRenderer destroyed.\n");
}
 
static void SWRenderer_beginFrame(Renderer* renderer, int32_t gameW, int32_t gameH, int32_t windowW, int32_t windowH)
{
    (void)renderer; (void)gameW; (void)gameH; (void)windowW; (void)windowH;
	fprintf(stderr, "%s\n", __func__);
}
 
static void SWRenderer_endFrame(Renderer* renderer)
{
    (void)renderer;
	fprintf(stderr, "%s\n", __func__);
}
 
static void SWRenderer_beginView(Renderer* renderer, int32_t viewX, int32_t viewY, int32_t viewW, int32_t viewH,
                                 int32_t portX, int32_t portY, int32_t portW, int32_t portH, float viewAngle)
{
    (void)renderer; (void)viewX; (void)viewY; (void)viewW; (void)viewH;
    (void)portX; (void)portY; (void)portW; (void)portH; (void)viewAngle;
	fprintf(stderr, "%s\n", __func__);
}
 
static void SWRenderer_endView(Renderer* renderer)
{
    (void)renderer;
	fprintf(stderr, "%s\n", __func__);
}
 
static void SWRenderer_beginGUI(Renderer* renderer, int32_t guiW, int32_t guiH,
                                int32_t portX, int32_t portY, int32_t portW, int32_t portH)
{
    (void)renderer; (void)guiW; (void)guiH;
    (void)portX; (void)portY; (void)portW; (void)portH;
	fprintf(stderr, "%s\n", __func__);
}
 
static void SWRenderer_endGUI(Renderer* renderer)
{
    (void)renderer;
	fprintf(stderr, "%s\n", __func__);
}
 
static void SWRenderer_drawSprite(Renderer* renderer, int32_t tpagIndex, float x, float y,
                                  float originX, float originY, float xscale, float yscale,
                                  float angleDeg, uint32_t color, float alpha)
{
    (void)renderer; (void)tpagIndex; (void)x; (void)y;
    (void)originX; (void)originY; (void)xscale; (void)yscale;
    (void)angleDeg; (void)color; (void)alpha;
}
 
static void SWRenderer_drawSpritePart(Renderer* renderer, int32_t tpagIndex,
                                      int32_t srcOffX, int32_t srcOffY, int32_t srcW, int32_t srcH,
                                      float x, float y, float xscale, float yscale, float angleDeg,
                                      float pivotX, float pivotY, uint32_t color, float alpha)
{
    (void)renderer; (void)tpagIndex; (void)srcOffX; (void)srcOffY; (void)srcW; (void)srcH;
    (void)x; (void)y; (void)xscale; (void)yscale; (void)angleDeg;
    (void)pivotX; (void)pivotY; (void)color; (void)alpha;
}
 
static void SWRenderer_drawSpritePos(Renderer* renderer, int32_t tpagIndex,
                                     float x1, float y1, float x2, float y2,
                                     float x3, float y3, float x4, float y4, float alpha)
{
    (void)renderer; (void)tpagIndex;
    (void)x1; (void)y1; (void)x2; (void)y2;
    (void)x3; (void)y3; (void)x4; (void)y4; (void)alpha;
}
 
static void SWRenderer_drawRectangle(Renderer* renderer, float x1, float y1, float x2, float y2,
                                     uint32_t color, float alpha, bool outline)
{
    (void)renderer; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)color; (void)alpha; (void)outline;
}
 
static void SWRenderer_drawRectangleColor(Renderer* renderer, float x1, float y1, float x2, float y2,
                                          uint32_t color1, uint32_t color2, uint32_t color3, uint32_t color4,
                                          float alpha, bool outline)
{
    (void)renderer; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)color1; (void)color2; (void)color3; (void)color4; (void)alpha; (void)outline;
}
 
static void SWRenderer_drawLine(Renderer* renderer, float x1, float y1, float x2, float y2,
                                float width, uint32_t color, float alpha)
{
    (void)renderer; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)width; (void)color; (void)alpha;
}
 
static void SWRenderer_drawTriangle(Renderer* renderer, float x1, float y1, float x2, float y2,
                                    float x3, float y3, bool outline)
{
    (void)renderer; (void)x1; (void)y1; (void)x2; (void)y2; (void)x3; (void)y3; (void)outline;
}
 
static void SWRenderer_drawLineColor(Renderer* renderer, float x1, float y1, float x2, float y2,
                                     float width, uint32_t color1, uint32_t color2, float alpha)
{
    (void)renderer; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)width; (void)color1; (void)color2; (void)alpha;
}
 
static void SWRenderer_drawText(Renderer* renderer, const char* text, float x, float y,
                                float xscale, float yscale, float angleDeg)
{
    (void)renderer; (void)text; (void)x; (void)y;
    (void)xscale; (void)yscale; (void)angleDeg;
}
 
static void SWRenderer_drawTextColor(Renderer* renderer, const char* text, float x, float y,
                                     float xscale, float yscale, float angleDeg,
                                     int32_t c1, int32_t c2, int32_t c3, int32_t c4, float alpha)
{
    (void)renderer; (void)text; (void)x; (void)y;
    (void)xscale; (void)yscale; (void)angleDeg;
    (void)c1; (void)c2; (void)c3; (void)c4; (void)alpha;
}
 
static void SWRenderer_flush(Renderer* renderer)
{
    (void)renderer;
}
 
static void SWRenderer_clearScreen(Renderer* renderer, uint32_t color, float alpha)
{
    (void)renderer; (void)color; (void)alpha;
}
 
static int32_t SWRenderer_createSpriteFromSurface(Renderer* renderer, int32_t surfaceID,
                                                   int32_t x, int32_t y, int32_t w, int32_t h,
                                                   bool removeback, bool smooth,
                                                   int32_t xorig, int32_t yorig)
{
    (void)renderer; (void)surfaceID; (void)x; (void)y; (void)w; (void)h;
    (void)removeback; (void)smooth; (void)xorig; (void)yorig;
    return 0;
}
 
static void SWRenderer_deleteSprite(Renderer* renderer, int32_t spriteIndex)
{
    (void)renderer; (void)spriteIndex;
}
 
static void SWRenderer_gpuSetBlendMode(Renderer* renderer, int32_t mode)
{
    (void)renderer; (void)mode;
}
 
static void SWRenderer_gpuSetBlendModeExt(Renderer* renderer, int32_t sfactor, int32_t dfactor)
{
    (void)renderer; (void)sfactor; (void)dfactor;
}
 
static void SWRenderer_gpuSetBlendEnable(Renderer* renderer, bool enable)
{
    (void)renderer; (void)enable;
}
 
static void SWRenderer_gpuSetAlphaTestEnable(Renderer* renderer, bool enable)
{
    (void)renderer; (void)enable;
}
 
static void SWRenderer_gpuSetAlphaTestRef(Renderer* renderer, uint8_t ref)
{
    (void)renderer; (void)ref;
}
 
static void SWRenderer_gpuSetColorWriteEnable(Renderer* renderer, bool red, bool green, bool blue, bool alpha)
{
    (void)renderer; (void)red; (void)green; (void)blue; (void)alpha;
}
 
static bool SWRenderer_gpuGetBlendEnable(Renderer* renderer)
{
    (void)renderer;
    return false;
}
 
static void SWRenderer_gpuSetFog(Renderer* renderer, bool enable, uint32_t color)
{
    (void)renderer; (void)enable; (void)color;
}
 
static void SWRenderer_drawTile(Renderer* renderer, RoomTile* tile, float offsetX, float offsetY)
{
    (void)renderer; (void)tile; (void)offsetX; (void)offsetY;
}
 
static void SWRenderer_drawTiled(Renderer* renderer, int32_t tpagIndex,
                                 float originX, float originY, float x, float y,
                                 float xscale, float yscale, bool tileX, bool tileY,
                                 float roomW, float roomH, uint32_t color, float alpha)
{
    (void)renderer; (void)tpagIndex; (void)originX; (void)originY; (void)x; (void)y;
    (void)xscale; (void)yscale; (void)tileX; (void)tileY;
    (void)roomW; (void)roomH; (void)color; (void)alpha;
}
 
static int32_t SWRenderer_createSurface(Renderer* renderer, int32_t width, int32_t height)
{
    (void)renderer; (void)width; (void)height;
    return 0;
}
 
static bool SWRenderer_surfaceExists(Renderer* renderer, int32_t surfaceID)
{
    (void)renderer; (void)surfaceID;
    return false;
}
 
static bool SWRenderer_setSurfaceTarget(Renderer* renderer, int32_t surfaceID)
{
    (void)renderer; (void)surfaceID;
    return false;
}
 
static bool SWRenderer_resetSurfaceTarget(Renderer* renderer)
{
    (void)renderer;
    return false;
}
 
static float SWRenderer_getSurfaceWidth(Renderer* renderer, int32_t surfaceID)
{
    (void)renderer; (void)surfaceID;
    return 0.0f;
}
 
static float SWRenderer_getSurfaceHeight(Renderer* renderer, int32_t surfaceID)
{
    (void)renderer; (void)surfaceID;
    return 0.0f;
}
 
static void SWRenderer_drawSurface(Renderer* renderer, int32_t surfaceID, float x, float y,
                                   float xscale, float yscale, float angleDeg,
                                   uint32_t color, float alpha)
{
    (void)renderer; (void)surfaceID; (void)x; (void)y;
    (void)xscale; (void)yscale; (void)angleDeg; (void)color; (void)alpha;
}
 
static void SWRenderer_drawSurfacePart(Renderer* renderer, int32_t surfaceID,
                                       int32_t x, int32_t y, int32_t left, int32_t top,
                                       int32_t width, int32_t height,
                                       float xscale, float yscale, uint32_t color, float alpha)
{
    (void)renderer; (void)surfaceID; (void)x; (void)y; (void)left; (void)top;
    (void)width; (void)height; (void)xscale; (void)yscale; (void)color; (void)alpha;
}
 
static void SWRenderer_drawSurfaceStretched(Renderer* renderer, int32_t surfaceID,
                                            float x, float y, float width, float height)
{
    (void)renderer; (void)surfaceID; (void)x; (void)y; (void)width; (void)height;
}
 
static void SWRenderer_surfaceResize(Renderer* renderer, int32_t surfaceID, int32_t width, int32_t height)
{
    (void)renderer; (void)surfaceID; (void)width; (void)height;
}
 
static void SWRenderer_surfaceFree(Renderer* renderer, int32_t surfaceID)
{
    (void)renderer; (void)surfaceID;
}
 
static void SWRenderer_surfaceCopy(Renderer* renderer,
                                   int32_t DestSurfaceID, int32_t DestX, int32_t DestY,
                                   int32_t SrcSurfaceID, int32_t SrcX, int32_t SrcY,
                                   int32_t SrcW, int32_t SrcH, bool part)
{
    (void)renderer;
    (void)DestSurfaceID; (void)DestX; (void)DestY;
    (void)SrcSurfaceID; (void)SrcX; (void)SrcY;
    (void)SrcW; (void)SrcH; (void)part;
}
 
static bool SWRenderer_surfaceGetPixels(Renderer* renderer, int32_t surfaceID, uint8_t* outRGBA)
{
    (void)renderer; (void)surfaceID; (void)outRGBA;
    return false;
}
 
static void SWRenderer_drawTiledPart(Renderer* renderer, int32_t tpagIndex,
                                     int32_t srcX, int32_t srcY, int32_t srcW, int32_t srcH,
                                     float dstX, float dstY, float dstW, float dstH,
                                     uint32_t color, float alpha)
{
    (void)renderer; (void)tpagIndex;
    (void)srcX; (void)srcY; (void)srcW; (void)srcH;
    (void)dstX; (void)dstY; (void)dstW; (void)dstH;
    (void)color; (void)alpha;
}
 
// ---------------------------------------------------------------------------
// Vtable registration
// ---------------------------------------------------------------------------
 
static RendererVtable swrVtable =
{
    .init                    = SWRenderer_init,
    .destroy                 = SWRenderer_destroy,
    .beginFrame              = SWRenderer_beginFrame,
    .endFrame                = SWRenderer_endFrame,
    .beginView               = SWRenderer_beginView,
    .endView                 = SWRenderer_endView,
    .beginGUI                = SWRenderer_beginGUI,
    .endGUI                  = SWRenderer_endGUI,
    .drawSprite              = SWRenderer_drawSprite,
    .drawSpritePart          = SWRenderer_drawSpritePart,
    .drawSpritePos           = SWRenderer_drawSpritePos,
    .drawRectangle           = SWRenderer_drawRectangle,
    .drawRectangleColor      = SWRenderer_drawRectangleColor,
    .drawLine                = SWRenderer_drawLine,
    .drawTriangle            = SWRenderer_drawTriangle,
    .drawLineColor           = SWRenderer_drawLineColor,
    .drawText                = SWRenderer_drawText,
    .drawTextColor           = SWRenderer_drawTextColor,
    .flush                   = SWRenderer_flush,
    .clearScreen             = SWRenderer_clearScreen,
    .createSpriteFromSurface = SWRenderer_createSpriteFromSurface,
    .deleteSprite            = SWRenderer_deleteSprite,
    .gpuSetBlendMode         = SWRenderer_gpuSetBlendMode,
    .gpuSetBlendModeExt      = SWRenderer_gpuSetBlendModeExt,
    .gpuSetBlendEnable       = SWRenderer_gpuSetBlendEnable,
    .gpuSetAlphaTestEnable   = SWRenderer_gpuSetAlphaTestEnable,
    .gpuSetAlphaTestRef      = SWRenderer_gpuSetAlphaTestRef,
    .gpuSetColorWriteEnable  = SWRenderer_gpuSetColorWriteEnable,
    .gpuGetBlendEnable       = SWRenderer_gpuGetBlendEnable,
    .gpuSetFog               = SWRenderer_gpuSetFog,
    .drawTile                = SWRenderer_drawTile,
    .drawTiled               = SWRenderer_drawTiled,
    .createSurface           = SWRenderer_createSurface,
    .surfaceExists           = SWRenderer_surfaceExists,
    .setSurfaceTarget        = SWRenderer_setSurfaceTarget,
    .resetSurfaceTarget      = SWRenderer_resetSurfaceTarget,
    .getSurfaceWidth         = SWRenderer_getSurfaceWidth,
    .getSurfaceHeight        = SWRenderer_getSurfaceHeight,
    .drawSurface             = SWRenderer_drawSurface,
    .drawSurfacePart         = SWRenderer_drawSurfacePart,
    .drawSurfaceStretched    = SWRenderer_drawSurfaceStretched,
    .surfaceResize           = SWRenderer_surfaceResize,
    .surfaceFree             = SWRenderer_surfaceFree,
    .surfaceCopy             = SWRenderer_surfaceCopy,
    .surfaceGetPixels        = SWRenderer_surfaceGetPixels,
    .drawTiledPart           = SWRenderer_drawTiledPart,
};

void SWRenderer_clearFrameBuffer(Renderer* renderer, uint32_t color)
{
	SWRenderer* swr = (SWRenderer*) renderer;
	
	size_t fbSize = swr->fbPitch;
	fbSize *= swr->height;
	for (size_t i = 0; i < fbSize; i++)
	{
		swr->fb[i] = color;
	}
}

Renderer* SWRenderer_create(int windowWidth, int windowHeight)
{
	SWRenderer* swr = safeCalloc(1, sizeof(SWRenderer));
	swr->base.vtable = &swrVtable;
	swr->base.drawColor = 0xFFFFFF;
	swr->base.drawAlpha = 1.0f;
	swr->base.drawFont = -1;
	swr->base.drawHalign = 0;
	swr->base.drawValign = 0;
	swr->base.circlePrecision = 24;
	
	swr->width = windowWidth;
	swr->height = windowHeight;

	return (Renderer*) swr;
}
