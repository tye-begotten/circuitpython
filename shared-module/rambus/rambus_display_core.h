#pragma once

#include "shared-module/displayio/display_core.h"
#include "shared-module/displayio/TileGrid.h"
#include "shared-module/displayio/Palette.h"
#include "shared-module/rambus/RAM.h"
#include "shared-module/vectorio/VectorShape.h"


bool rambus_display_core_fill_area(displayio_display_core_t *self, displayio_area_t *area, uint32_t *mask, rambus_ptr_t *addr);
bool rambus_group_fill_area(displayio_group_t *self, const _displayio_colorspace_t *colorspace, 
    const displayio_area_t *area, uint32_t *mask, rambus_ptr_t *addr);
bool rambus_tilegrid_fill_area(displayio_tilegrid_t *self,
    const _displayio_colorspace_t *colorspace, const displayio_area_t *area,
    uint32_t *mask, rambus_ptr_t *addr);
bool rambus_vector_shape_fill_area(vectorio_vector_shape_t *self, const _displayio_colorspace_t *colorspace, 
    const displayio_area_t *area, uint32_t *mask, rambus_ptr_t *addr);