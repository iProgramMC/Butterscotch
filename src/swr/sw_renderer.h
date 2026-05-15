#pragma once

#include <stdint.h>
#include "renderer.h"

typedef struct {
	int16_t x1, y1, x2, y2;
}
SWRectangle;

#define swrSetRect(rc, _x1, _y1, _x2, _y2) do { \
	(rc)->x1 = (_x1); \
	(rc)->y1 = (_y1); \
	(rc)->x2 = (_x2); \
	(rc)->y2 = (_y2); \
} while (0)

Renderer* SWRenderer_create(int windowWidth, int windowHeight);

void SWRenderer_clearFrameBuffer(Renderer* renderer, uint32_t color);

SWRectangle* SWRenderer_getUpdateRects(Renderer* renderer, int* rectCount, bool* overflow);
