/**
 * Copyright (c) 2021 Jeff Boody
 * Copyright (c) 2018 Eric Bruneton
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef atmo_renderer_H
#define atmo_renderer_H

#include "libvkk/vkk.h"

#define ATMO_TOUCH_MODE_VIEW 0
#define ATMO_TOUCH_MODE_SUN  1

#define ATMO_TOUCH_EVENT_UP   0
#define ATMO_TOUCH_EVENT_DOWN 1
#define ATMO_TOUCH_EVENT_MOVE 2

/***********************************************************
* public                                                   *
***********************************************************/

typedef struct atmo_renderer_s
{
	vkk_engine_t* engine;

	// screen state
	uint32_t screen_w;
	uint32_t screen_h;
	float    density;

	// content rect
	uint32_t content_rect_top;
	uint32_t content_rect_left;
	uint32_t content_rect_width;
	uint32_t content_rect_height;

	// escape state
	double escape_t0;

	// touch state
	int   touch_mode;
	int   touch_event;
	float touch_x;
	float touch_y;

	// demo state
	// view distance in meters
	// view/sun angles in radians
	float view_distance;
	float view_zenith;
	float view_azimuth;
	float sun_zenith;
	float sun_azimuth;
	float exposure;

	// graphics state
	int simple;
	vkk_uniformSetFactory_t* usf0_state;
	vkk_uniformSetFactory_t* usf1_tex;
	vkk_pipelineLayout_t*    pl;
	vkk_graphicsPipeline_t*  gp_default;
	vkk_graphicsPipeline_t*  gp_simple;
	vkk_buffer_t*            vb0_vertex;
	vkk_buffer_t*            ub00_mvp;
	vkk_buffer_t*            ub01_model_from_view;
	vkk_buffer_t*            ub02_view_from_clip;
	vkk_buffer_t*            ub03_camera;
	vkk_buffer_t*            ub04_exposure;
	vkk_buffer_t*            ub05_white_point;
	vkk_buffer_t*            ub06_earth_center;
	vkk_buffer_t*            ub07_sun_direction;
	vkk_buffer_t*            ub08_sun_size;
	vkk_image_t*             img10_transmittance;
	vkk_image_t*             img112_scattering;
	vkk_image_t*             img13_irradiance;
	vkk_uniformSet_t*        us0_state;
	vkk_uniformSet_t*        us1_tex;
} atmo_renderer_t;

atmo_renderer_t* atmo_renderer_new(vkk_engine_t* engine);
void             atmo_renderer_delete(atmo_renderer_t** _self);
void             atmo_renderer_density(atmo_renderer_t* self,
                                       float density);
void             atmo_renderer_draw(atmo_renderer_t* self);
void             atmo_renderer_touch(atmo_renderer_t* self,
                                     int event,
                                     float x,
                                     float y);
void             atmo_renderer_keyPress(atmo_renderer_t* self,
                                        int keycode, int meta);
void             atmo_renderer_contentRect(atmo_renderer_t* self,
                                           uint32_t t,
                                           uint32_t l,
                                           uint32_t b,
                                           uint32_t r);

#endif
