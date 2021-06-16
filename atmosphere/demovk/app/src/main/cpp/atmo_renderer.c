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

#include <math.h>
#include <stdlib.h>

#define LOG_TAG "atmo"
#include "libcc/math/cc_mat4f.h"
#include "libcc/math/cc_vec2f.h"
#include "libcc/math/cc_vec4f.h"
#include "libcc/cc_log.h"
#include "libcc/cc_memory.h"
#include "libcc/cc_timestamp.h"
#include "libvkk/vkk_platform.h"
#include "libpak/pak_file.h"
#include "atmo_renderer.h"

// The following constants must have the same values as in
// constants.h and demo.cc

const int TRANSMITTANCE_TEXTURE_WIDTH  = 256;
const int TRANSMITTANCE_TEXTURE_HEIGHT = 64;
const int SCATTERING_TEXTURE_WIDTH     = 256;
const int SCATTERING_TEXTURE_HEIGHT    = 128;
const int SCATTERING_TEXTURE_DEPTH     = 32;
const int IRRADIANCE_TEXTURE_WIDTH     = 64;
const int IRRADIANCE_TEXTURE_HEIGHT    = 16;
const int TEXTURE_COMPONENTS           = 4;

const float kSunAngularRadius   = 0.00935f/2.0f;
const float kSunSolidAngle      = M_PI*kSunAngularRadius*kSunAngularRadius;
const float kLengthUnitInMeters = 1000.0f;

/***********************************************************
* private                                                  *
***********************************************************/

static void
atmo_renderer_setView(atmo_renderer_t* self,
                      float view_distance,
                      float view_zenith,
                      float view_azimuth,
                      float sun_zenith,
                      float sun_azimuth,
                      float exposure)
{
	ASSERT(self);

	self->view_distance = view_distance;
	self->view_zenith   = view_zenith;
	self->view_azimuth  = view_azimuth;
	self->sun_zenith    = sun_zenith;
	self->sun_azimuth   = sun_azimuth;
	self->exposure      = exposure;
}

static int
atmo_renderer_loadDat(atmo_renderer_t* self,
                      float* dat_transmittance,
                      float* dat_scattering,
                      float* dat_irradiance)
{
	ASSERT(self);
	ASSERT(dat_transmittance);
	ASSERT(dat_scattering);
	ASSERT(dat_irradiance);

	char resource[256];
	snprintf(resource, 256, "%s/resource.pak",
	         vkk_engine_internalPath(self->engine));

	pak_file_t* pak;
	pak = pak_file_open(resource, PAK_FLAG_READ);
	if(pak == NULL)
	{
		return 0;
	}

	int size;
	int bytes;
	size  = pak_file_seek(pak, "dat/transmittance.dat");
	bytes = 4*sizeof(float)*
	        TRANSMITTANCE_TEXTURE_WIDTH*
	        TRANSMITTANCE_TEXTURE_HEIGHT;
	if((size == 0) || (size != bytes))
	{
		LOGE("invalid size=%i, bytes=%i", size, bytes);
		goto failure;
	}

	if(pak_file_read(pak, (void*) dat_transmittance,
	                 size, 1) != 1)
	{
		goto failure;
	}

	size  = pak_file_seek(pak, "dat/scattering.dat");
	bytes = 4*sizeof(float)*
	        SCATTERING_TEXTURE_WIDTH*
	        SCATTERING_TEXTURE_HEIGHT*
	        SCATTERING_TEXTURE_DEPTH;
	if((size == 0) || (size != bytes))
	{
		LOGE("invalid size=%i, bytes=%i", size, bytes);
		goto failure;
	}

	if(pak_file_read(pak, (void*) dat_scattering,
	                 size, 1) != 1)
	{
		goto failure;
	}

	size  = pak_file_seek(pak, "dat/irradiance.dat");
	bytes = 4*sizeof(float)*
	        IRRADIANCE_TEXTURE_WIDTH*
	        IRRADIANCE_TEXTURE_HEIGHT;
	if((size == 0) || (size != bytes))
	{
		LOGE("invalid size=%i, bytes=%i", size, bytes);
		goto failure;
	}

	if(pak_file_read(pak, (void*) dat_irradiance,
	                 size, 1) != 1)
	{
		goto failure;
	}

	pak_file_close(&pak);

	// success
	return 1;

	// failure
	failure:
		pak_file_close(&pak);
	return 0;
}

/***********************************************************
* public                                                   *
***********************************************************/

atmo_renderer_t*
atmo_renderer_new(vkk_engine_t* engine)
{
	ASSERT(engine);

	atmo_renderer_t* self;
	self = CALLOC(1, sizeof(atmo_renderer_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	self->engine    = engine;
	self->escape_t0 = cc_timestamp();
	self->density   = 1.0f;

	self->view_distance = 9000.0f;
	self->view_zenith   = 1.47f;
	self->view_azimuth  = -0.1f;
	self->sun_zenith    = 1.3f;
	self->sun_azimuth   = 2.9f;
	self->exposure      = 10.0f;

	int bytes;
	bytes = sizeof(float)*TEXTURE_COMPONENTS*
	        TRANSMITTANCE_TEXTURE_WIDTH*
	        TRANSMITTANCE_TEXTURE_HEIGHT;

	float* dat_transmittance;
	dat_transmittance = (float*) MALLOC(bytes);
	if(dat_transmittance == NULL)
	{
		LOGE("MALLOC failed");
		goto fail_dat_transmittance;
	}

	bytes = sizeof(float)*TEXTURE_COMPONENTS*
	        SCATTERING_TEXTURE_WIDTH*
	        SCATTERING_TEXTURE_HEIGHT*
	        SCATTERING_TEXTURE_DEPTH;

	float* dat_scattering;
	dat_scattering = (float*) MALLOC(bytes);
	if(dat_scattering == NULL)
	{
		LOGE("MALLOC failed");
		goto fail_dat_scattering;
	}

	bytes = sizeof(float)*TEXTURE_COMPONENTS*
	        IRRADIANCE_TEXTURE_WIDTH*
	        IRRADIANCE_TEXTURE_HEIGHT;

	float* dat_irradiance;
	dat_irradiance = (float*) MALLOC(bytes);
	if(dat_irradiance == NULL)
	{
		LOGE("MALLOC failed");
		goto fail_dat_irradiance;
	}

	vkk_uniformBinding_t ub_array0[] =
	{
		// layout(std140, set=0, binding=0) uniform uniformMvp
		{
			.binding = 0,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.stage   = VKK_STAGE_VS,
		},
		// layout(std140, set=0, binding=1) uniform uniformModelFromView
		{
			.binding = 1,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.stage   = VKK_STAGE_VS,
		},
		// layout(std140, set=0, binding=2) uniform uniformViewFromClip
		{
			.binding = 2,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.stage   = VKK_STAGE_VS,
		},
		// layout(std140, set=0, binding=3) uniform uniformCamera
		{
			.binding = 3,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.stage   = VKK_STAGE_FS,
		},
		// layout(std140, set=0, binding=4) uniform uniformExposure
		{
			.binding = 4,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.stage   = VKK_STAGE_FS,
		},
		// layout(std140, set=0, binding=5) uniform uniformWhitePoint
		{
			.binding = 5,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.stage   = VKK_STAGE_FS,
		},
		// layout(std140, set=0, binding=6) uniform uniformEarthCenter
		{
			.binding = 6,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.stage   = VKK_STAGE_FS,
		},
		// layout(std140, set=0, binding=7) uniform uniformSunDirection
		{
			.binding = 7,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.stage   = VKK_STAGE_FS,
		},
		// layout(std140, set=0, binding=8) uniform uniformSunSize
		{
			.binding = 8,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.stage   = VKK_STAGE_FS,
		},
	};

	self->usf0_state = vkk_uniformSetFactory_new(self->engine,
	                                             VKK_UPDATE_MODE_DEFAULT,
	                                             9, ub_array0);
	if(self->usf0_state == NULL)
	{
		goto fail_usf0;
	}

	vkk_uniformBinding_t ub_array1[] =
	{
		// layout(set=1, binding=0) uniform sampler2D transmittance_texture;
		{
			.binding = 0,
			.type    = VKK_UNIFORM_TYPE_IMAGE,
			.stage   = VKK_STAGE_FS,
			.si      =
			{
				VKK_SAMPLER_FILTER_LINEAR,
				VKK_SAMPLER_FILTER_LINEAR,
				VKK_SAMPLER_MIPMAP_MODE_LINEAR,
			}
		},
		// layout(set=1, binding=1) uniform sampler3D scattering_texture;
		{
			.binding = 1,
			.type    = VKK_UNIFORM_TYPE_IMAGE,
			.stage   = VKK_STAGE_FS,
			.si      =
			{
				VKK_SAMPLER_FILTER_LINEAR,
				VKK_SAMPLER_FILTER_LINEAR,
				VKK_SAMPLER_MIPMAP_MODE_LINEAR,
			}
		},
		// layout(set=1, binding=2) uniform sampler3D single_mie_scattering_texture;
		{
			.binding = 2,
			.type    = VKK_UNIFORM_TYPE_IMAGE,
			.stage   = VKK_STAGE_FS,
			.si      =
			{
				VKK_SAMPLER_FILTER_LINEAR,
				VKK_SAMPLER_FILTER_LINEAR,
				VKK_SAMPLER_MIPMAP_MODE_LINEAR,
			}
		},
		// layout(set=1, binding=3) uniform sampler2D irradiance_texture;
		{
			.binding = 3,
			.type    = VKK_UNIFORM_TYPE_IMAGE,
			.stage   = VKK_STAGE_FS,
			.si      =
			{
				VKK_SAMPLER_FILTER_LINEAR,
				VKK_SAMPLER_FILTER_LINEAR,
				VKK_SAMPLER_MIPMAP_MODE_LINEAR,
			}
		},
	};

	self->usf1_tex = vkk_uniformSetFactory_new(self->engine,
	                                           VKK_UPDATE_MODE_STATIC,
	                                           4, ub_array1);
	if(self->usf1_tex == NULL)
	{
		goto fail_usf1;
	}

	vkk_uniformSetFactory_t* usf_array[] =
	{
		self->usf0_state,
		self->usf1_tex,
	};

	self->pl = vkk_pipelineLayout_new(self->engine,
	                                  2, usf_array);
	if(self->pl == NULL)
	{
		goto fail_pl;
	}

	vkk_renderer_t* primary;
	primary = vkk_engine_defaultRenderer(self->engine);

	vkk_vertexBufferInfo_t vbi[] =
	{
		// layout(location=0) in vec4 vertex;
		{
			.location   = 0,
			.components = 4,
			.format     = VKK_VERTEX_FORMAT_FLOAT
		},
	};

	vkk_graphicsPipelineInfo_t gpi =
	{
		.renderer          = primary,
		.pl                = self->pl,
		.vs                = "shaders/default_vert.spv",
		.fs                = "shaders/default_frag.spv",
		.vb_count          = 1,
		.vbi               = vbi,
		.primitive         = VKK_PRIMITIVE_TRIANGLE_STRIP,
		.primitive_restart = 0,
		.cull_back         = 0,
		.depth_test        = 1,
		.depth_write       = 1,
		.blend_mode        = VKK_BLEND_MODE_DISABLED
	};

	self->gp_default = vkk_graphicsPipeline_new(self->engine,
	                                            &gpi);
	if(self->gp_default == NULL)
	{
		goto fail_gp_default;
	}

	gpi.vs = "shaders/default_vert.spv",
	gpi.fs = "shaders/simple_frag.spv",
	self->gp_simple = vkk_graphicsPipeline_new(self->engine,
	                                           &gpi);
	if(self->gp_simple == NULL)
	{
		goto fail_gp_simple;
	}

	float vertices[] =
	{
		-1.0f, -1.0f, -1.0f, 1.0f,
		 1.0f, -1.0f, -1.0f, 1.0f,
		-1.0f,  1.0f, -1.0f, 1.0f,
		 1.0f,  1.0f, -1.0f, 1.0f,
	};

	self->vb0_vertex = vkk_buffer_new(self->engine,
	                                  VKK_UPDATE_MODE_STATIC,
	                                  VKK_BUFFER_USAGE_VERTEX,
	                                  sizeof(vertices),
	                                  (const void*) vertices);
	if(self->vb0_vertex == NULL)
	{
		goto fail_vb0_vertex;
	}

	self->ub00_mvp = vkk_buffer_new(self->engine,
	                                VKK_UPDATE_MODE_DEFAULT,
	                                VKK_BUFFER_USAGE_UNIFORM,
	                                sizeof(cc_mat4f_t),
	                                NULL);
	if(self->ub00_mvp == NULL)
	{
		goto fail_ub00;
	}

	self->ub01_model_from_view = vkk_buffer_new(self->engine,
	                                            VKK_UPDATE_MODE_DEFAULT,
	                                            VKK_BUFFER_USAGE_UNIFORM,
	                                            sizeof(cc_mat4f_t),
	                                            NULL);
	if(self->ub01_model_from_view == NULL)
	{
		goto fail_ub01;
	}

	self->ub02_view_from_clip = vkk_buffer_new(self->engine,
	                                           VKK_UPDATE_MODE_DEFAULT,
	                                           VKK_BUFFER_USAGE_UNIFORM,
	                                           sizeof(cc_mat4f_t),
	                                           NULL);
	if(self->ub02_view_from_clip == NULL)
	{
		goto fail_ub02;
	}

	self->ub03_camera = vkk_buffer_new(self->engine,
	                                   VKK_UPDATE_MODE_DEFAULT,
	                                   VKK_BUFFER_USAGE_UNIFORM,
	                                   sizeof(cc_vec4f_t),
	                                   NULL);
	if(self->ub03_camera == NULL)
	{
		goto fail_ub03;
	}

	self->ub04_exposure = vkk_buffer_new(self->engine,
	                                     VKK_UPDATE_MODE_DEFAULT,
	                                     VKK_BUFFER_USAGE_UNIFORM,
	                                     sizeof(float),
	                                     NULL);
	if(self->ub04_exposure == NULL)
	{
		goto fail_ub04;
	}

	self->ub05_white_point = vkk_buffer_new(self->engine,
	                                        VKK_UPDATE_MODE_DEFAULT,
	                                        VKK_BUFFER_USAGE_UNIFORM,
	                                        sizeof(cc_vec4f_t),
	                                        NULL);
	if(self->ub05_white_point == NULL)
	{
		goto fail_ub05;
	}

	self->ub06_earth_center = vkk_buffer_new(self->engine,
	                                         VKK_UPDATE_MODE_DEFAULT,
	                                         VKK_BUFFER_USAGE_UNIFORM,
	                                         sizeof(cc_vec4f_t),
	                                         NULL);
	if(self->ub06_earth_center == NULL)
	{
		goto fail_ub06;
	}

	self->ub07_sun_direction = vkk_buffer_new(self->engine,
	                                          VKK_UPDATE_MODE_DEFAULT,
	                                          VKK_BUFFER_USAGE_UNIFORM,
	                                          sizeof(cc_vec4f_t),
	                                          NULL);
	if(self->ub07_sun_direction == NULL)
	{
		goto fail_ub07;
	}

	self->ub08_sun_size = vkk_buffer_new(self->engine,
	                                     VKK_UPDATE_MODE_DEFAULT,
	                                     VKK_BUFFER_USAGE_UNIFORM,
	                                     sizeof(cc_vec2f_t),
	                                     NULL);
	if(self->ub08_sun_size == NULL)
	{
		goto fail_ub08;
	}

	if(atmo_renderer_loadDat(self,
	                         dat_transmittance,
	                         dat_scattering,
	                         dat_irradiance) == 0)
	{
		goto fail_dat;
	}

	vkk_imageCaps_t caps;
	vkk_engine_imageCaps(self->engine,
	                     VKK_IMAGE_FORMAT_RGBAF32, &caps);
	if((caps.texture == 0) ||
	   (caps.filter_linear == 0))
	{
		goto fail_format;
	}

	self->img10_transmittance =
		vkk_image_new(engine,
	                  TRANSMITTANCE_TEXTURE_WIDTH,
	                TRANSMITTANCE_TEXTURE_HEIGHT, 1,
	                  VKK_IMAGE_FORMAT_RGBAF32, 0,
	                  VKK_STAGE_FS, dat_transmittance);
	if(self->img10_transmittance == NULL)
	{
		goto fail_img10;
	}

	self->img112_scattering =
		vkk_image_new(engine,
	                  SCATTERING_TEXTURE_WIDTH,
	                  SCATTERING_TEXTURE_HEIGHT,
	                  SCATTERING_TEXTURE_DEPTH,
	                  VKK_IMAGE_FORMAT_RGBAF32, 0,
	                  VKK_STAGE_FS, dat_scattering);
	if(self->img112_scattering == NULL)
	{
		goto fail_img112;
	}

	self->img13_irradiance =
		vkk_image_new(engine,
	                  IRRADIANCE_TEXTURE_WIDTH,
	                  IRRADIANCE_TEXTURE_HEIGHT, 1,
	                  VKK_IMAGE_FORMAT_RGBAF32, 0,
	                  VKK_STAGE_FS, dat_irradiance);
	if(self->img13_irradiance == NULL)
	{
		goto fail_img13;
	}

	vkk_uniformAttachment_t ua_array0[] =
	{
		// layout(std140, set=0, binding=0) uniform uniformMvp
		{
			.binding = 0,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.buffer  = self->ub00_mvp
		},
		// layout(std140, set=0, binding=1) uniform uniformModelFromView
		{
			.binding = 1,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.buffer  = self->ub01_model_from_view
		},
		// layout(std140, set=0, binding=2) uniform uniformViewFromClip
		{
			.binding = 2,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.buffer  = self->ub02_view_from_clip
		},
		// layout(std140, set=0, binding=3) uniform uniformCamera
		{
			.binding = 3,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.buffer  = self->ub03_camera
		},
		// layout(std140, set=0, binding=4) uniform uniformExposure
		{
			.binding = 4,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.buffer  = self->ub04_exposure
		},
		// layout(std140, set=0, binding=5) uniform uniformWhitePoint
		{
			.binding = 5,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.buffer  = self->ub05_white_point
		},
		// layout(std140, set=0, binding=6) uniform uniformEarthCenter
		{
			.binding = 6,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.buffer  = self->ub06_earth_center
		},
		// layout(std140, set=0, binding=7) uniform uniformSunDirection
		{
			.binding = 7,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.buffer  = self->ub07_sun_direction
		},
		// layout(std140, set=0, binding=8) uniform uniformSunSize
		{
			.binding = 8,
			.type    = VKK_UNIFORM_TYPE_BUFFER,
			.buffer  = self->ub08_sun_size
		},
	};

	self->us0_state = vkk_uniformSet_new(self->engine, 0, 9,
	                                     ua_array0,
	                                     self->usf0_state);
	if(self->us0_state == NULL)
	{
		goto fail_us0;
	}

	vkk_uniformAttachment_t ua_array1[] =
	{
		// layout(set=1, binding=0) uniform sampler2D transmittance_texture;
		{
			.binding = 0,
			.type    = VKK_UNIFORM_TYPE_IMAGE,
			.image   = self->img10_transmittance
		},
		// layout(set=1, binding=1) uniform sampler3D scattering_texture;
		{
			.binding = 1,
			.type    = VKK_UNIFORM_TYPE_IMAGE,
			.image   = self->img112_scattering
		},
		// layout(set=1, binding=2) uniform sampler3D single_mie_scattering_texture;
		{
			.binding = 2,
			.type    = VKK_UNIFORM_TYPE_IMAGE,
			.image   = self->img112_scattering
		},
		// layout(set=1, binding=3) uniform sampler2D irradiance_texture;
		{
			.binding = 3,
			.type    = VKK_UNIFORM_TYPE_IMAGE,
			.image   = self->img13_irradiance
		},
	};

	self->us1_tex = vkk_uniformSet_new(self->engine, 1, 4,
	                                   ua_array1,
	                                   self->usf1_tex);
	if(self->us1_tex == NULL)
	{
		goto fail_us1;
	}

	FREE(dat_irradiance);
	FREE(dat_scattering);
	FREE(dat_transmittance);

	// success
	return self;

	// failure
	fail_us1:
		vkk_uniformSet_delete(&self->us0_state);
	fail_us0:
		vkk_image_delete(&self->img13_irradiance);
	fail_img13:
		vkk_image_delete(&self->img112_scattering);
	fail_img112:
		vkk_image_delete(&self->img10_transmittance);
	fail_img10:
	fail_format:
	fail_dat:
		vkk_buffer_delete(&self->ub08_sun_size);
	fail_ub08:
		vkk_buffer_delete(&self->ub07_sun_direction);
	fail_ub07:
		vkk_buffer_delete(&self->ub06_earth_center);
	fail_ub06:
		vkk_buffer_delete(&self->ub05_white_point);
	fail_ub05:
		vkk_buffer_delete(&self->ub04_exposure);
	fail_ub04:
		vkk_buffer_delete(&self->ub03_camera);
	fail_ub03:
		vkk_buffer_delete(&self->ub02_view_from_clip);
	fail_ub02:
		vkk_buffer_delete(&self->ub01_model_from_view);
	fail_ub01:
		vkk_buffer_delete(&self->ub00_mvp);
	fail_ub00:
		vkk_buffer_delete(&self->vb0_vertex);
	fail_vb0_vertex:
		vkk_graphicsPipeline_delete(&self->gp_simple);
	fail_gp_simple:
		vkk_graphicsPipeline_delete(&self->gp_default);
	fail_gp_default:
		vkk_pipelineLayout_delete(&self->pl);
	fail_pl:
		vkk_uniformSetFactory_delete(&self->usf1_tex);
	fail_usf1:
		vkk_uniformSetFactory_delete(&self->usf0_state);
	fail_usf0:
		FREE(dat_irradiance);
	fail_dat_irradiance:
		FREE(dat_scattering);
	fail_dat_scattering:
		FREE(dat_transmittance);
	fail_dat_transmittance:
		FREE(self);
	return NULL;
}

void atmo_renderer_delete(atmo_renderer_t** _self)
{
	ASSERT(_self);

	atmo_renderer_t* self = *_self;
	if(self)
	{
		vkk_uniformSet_delete(&self->us1_tex);
		vkk_uniformSet_delete(&self->us0_state);
		vkk_image_delete(&self->img13_irradiance);
		vkk_image_delete(&self->img112_scattering);
		vkk_image_delete(&self->img10_transmittance);
		vkk_buffer_delete(&self->ub08_sun_size);
		vkk_buffer_delete(&self->ub07_sun_direction);
		vkk_buffer_delete(&self->ub06_earth_center);
		vkk_buffer_delete(&self->ub05_white_point);
		vkk_buffer_delete(&self->ub04_exposure);
		vkk_buffer_delete(&self->ub03_camera);
		vkk_buffer_delete(&self->ub02_view_from_clip);
		vkk_buffer_delete(&self->ub01_model_from_view);
		vkk_buffer_delete(&self->ub00_mvp);
		vkk_buffer_delete(&self->vb0_vertex);
		vkk_graphicsPipeline_delete(&self->gp_simple);
		vkk_graphicsPipeline_delete(&self->gp_default);
		vkk_pipelineLayout_delete(&self->pl);
		vkk_uniformSetFactory_delete(&self->usf1_tex);
		vkk_uniformSetFactory_delete(&self->usf0_state);
		FREE(self);
		*_self = NULL;
	}
}

void atmo_renderer_density(atmo_renderer_t* self,
                           float density)
{
	ASSERT(self);

	self->density = density;
}

void atmo_renderer_draw(atmo_renderer_t* self)
{
	ASSERT(self);

	vkk_renderer_t* rend;
	rend = vkk_engine_defaultRenderer(self->engine);

	float clear_color[4] =
	{
		1.0f, 0.0f, 1.0f, 1.0f
	};

	if(vkk_renderer_beginDefault(rend,
	                             VKK_RENDERER_MODE_PRIMARY,
	                             clear_color) == 0)
	{
		return;
	}

	vkk_renderer_surfaceSize(rend,
	                         &self->screen_w,
	                         &self->screen_h);

	// update screen, viewport and scissor
	float screen_x = 0.0f;
	float screen_y = 0.0f;
	float screen_w = (float) self->screen_w;
	float screen_h = (float) self->screen_h;
	if((self->content_rect_width  > 0) &&
	   (self->content_rect_height > 0) &&
	   (self->content_rect_width  <= self->screen_w) &&
	   (self->content_rect_height <= self->screen_h))
	{
		screen_x = (float) self->content_rect_left;
		screen_y = (float) self->content_rect_top;
		screen_w = (float) self->content_rect_width;
		screen_h = (float) self->content_rect_height;

		vkk_renderer_viewport(rend, screen_x, screen_y,
		                      screen_w, screen_h);
		vkk_renderer_scissor(rend,
		                     self->content_rect_left,
		                     self->content_rect_top,
		                     self->content_rect_width,
		                     self->content_rect_height);
	}

	// setup demo
	float kFovY       = 50.0f/180.0f*M_PI;
	float kTanFovY    = tan(kFovY/2.0f);
	float aspectRatio = screen_w/screen_h;

	cc_mat4f_t view_from_clip =
	{
		.m00=kTanFovY*aspectRatio,
		.m11=kTanFovY,
		.m23=-1.0f,
		.m32=1.0f,
		.m33=1.0f,
	};

	float cosZV = cosf(self->view_zenith);
	float sinZV = sinf(self->view_zenith);
	float cosAV = cosf(self->view_azimuth);
	float sinAV = sinf(self->view_azimuth);
	float distV = self->view_distance/kLengthUnitInMeters;

	cc_mat4f_t model_from_view =
	{
		.m00=-sinAV,
		.m01=-cosZV*cosAV,
		.m02=sinZV*cosAV,
		.m03=sinZV*cosAV*distV,
		.m10=cosAV,
		.m11=-cosZV*sinAV,
		.m12=sinZV*sinAV,
		.m13=sinZV*sinAV*distV,
		.m21=sinZV,
		.m22=cosZV,
		.m23=cosZV*distV,
		.m33=1.0f
	};

	cc_vec4f_t camera =
	{
		.x=model_from_view.m03,
		.y=model_from_view.m13,
		.z=model_from_view.m23,
		.w=model_from_view.m33,
	};

	cc_vec4f_t white_point =
	{
		.x=1.0f,
		.y=1.0f,
		.z=1.0f,
		.w=1.0f,
	};

	cc_vec4f_t earth_center =
	{
		.z=-6360000.0f/kLengthUnitInMeters,
		.w=1.0f
	};

	float cosZS = cosf(self->sun_zenith);
	float sinZS = sinf(self->sun_zenith);
	float cosAS = cosf(self->sun_azimuth);
	float sinAS = sinf(self->sun_azimuth);

	cc_vec4f_t sun_direction =
	{
		.x=cosAS*sinZS,
		.y=sinAS*sinZS,
		.z=cosZS,
		.w=1.0f
	};

	cc_vec2f_t sun_size =
	{
		.x=tanf(kSunAngularRadius),
		.y=cosf(kSunAngularRadius)
	};

	cc_mat4f_t mvp;
	cc_mat4f_orthoVK(&mvp, 1, -1.0f, 1.0f,
	                 -1.0f, 1.0f, 0.0f, 2.0f);

	// draw demo
	vkk_uniformSet_t* us_array[2] =
	{
		self->us0_state,
		self->us1_tex,
	};

	vkk_renderer_updateBuffer(rend, self->ub00_mvp,
	                          sizeof(cc_mat4f_t),
	                          (const void*) &mvp);
	vkk_renderer_updateBuffer(rend, self->ub01_model_from_view,
	                          sizeof(cc_mat4f_t),
	                          (const void*) &model_from_view);
	vkk_renderer_updateBuffer(rend, self->ub02_view_from_clip,
	                          sizeof(cc_mat4f_t),
	                          (const void*) &view_from_clip);
	vkk_renderer_updateBuffer(rend, self->ub03_camera,
	                          sizeof(cc_vec4f_t),
	                          (const void*) &camera);
	vkk_renderer_updateBuffer(rend, self->ub04_exposure,
	                          sizeof(float),
	                          (const void*) &self->exposure);
	vkk_renderer_updateBuffer(rend, self->ub05_white_point,
	                          sizeof(cc_vec4f_t),
	                          (const void*) &white_point);
	vkk_renderer_updateBuffer(rend, self->ub06_earth_center,
	                          sizeof(cc_vec4f_t),
	                          (const void*) &earth_center);
	vkk_renderer_updateBuffer(rend, self->ub07_sun_direction,
	                          sizeof(cc_vec4f_t),
	                          (const void*) &sun_direction);
	vkk_renderer_updateBuffer(rend, self->ub08_sun_size,
	                          sizeof(cc_vec2f_t),
	                          (const void*) &sun_size);
	if(self->simple)
	{
		vkk_renderer_bindGraphicsPipeline(rend, self->gp_simple);
	}
	else
	{
		vkk_renderer_bindGraphicsPipeline(rend, self->gp_default);
	}
	vkk_renderer_bindUniformSets(rend, 2, us_array);
	vkk_renderer_draw(rend, 4, 1, &self->vb0_vertex);
	vkk_renderer_end(rend);
}

void atmo_renderer_touch(atmo_renderer_t* self,
                         int event, float x, float y)
{
	ASSERT(self);

	if(self->touch_event == ATMO_TOUCH_EVENT_UP)
	{
		if(event == ATMO_TOUCH_EVENT_DOWN)
		{
			self->touch_event = event;
			self->touch_x     = x;
			self->touch_y     = y;
		}
	}
	else if(event == ATMO_TOUCH_EVENT_UP)
	{
		self->touch_event = event;
	}
	else if((self->touch_event == ATMO_TOUCH_EVENT_DOWN) &&
	        (event == ATMO_TOUCH_EVENT_MOVE))
	{
		float kScale = 500.0f;
		if(self->touch_mode == ATMO_TOUCH_MODE_SUN)
		{
			self->sun_zenith -= (self->touch_y - y)/kScale;
			if(self->sun_zenith > M_PI)
			{
				self->sun_zenith = M_PI;
			}

			if(self->sun_zenith < 0.0f)
			{
				self->sun_zenith = 0.0f;
			}

			self->sun_azimuth += (self->touch_x - x)/kScale;
			while(self->sun_azimuth > 2.0f*M_PI)
			{
				self->sun_azimuth -= 2.0f*M_PI;
			}

			while(self->sun_azimuth < 0.0f)
			{
				self->sun_azimuth += 2.0f*M_PI;
			}
		}
		else if(self->touch_mode == ATMO_TOUCH_MODE_VIEW)
		{
			self->view_zenith += (self->touch_y - y)/kScale;
			if(self->view_zenith > M_PI/2.0f)
			{
				self->view_zenith = M_PI/2.0f;
			}

			if(self->view_zenith < 0.0f)
			{
				self->view_zenith = 0.0f;
			}

			self->view_azimuth += (self->touch_x - x)/kScale;
			while(self->view_azimuth > 2.0f*M_PI)
			{
				self->view_azimuth -= 2.0f*M_PI;
			}

			while(self->view_azimuth < 0.0f)
			{
				self->view_azimuth += 2.0f*M_PI;
			}
		}

		self->touch_x = x;
		self->touch_y = y;
	}
}

void atmo_renderer_keyPress(atmo_renderer_t* self,
                            int keycode, int meta)
{
	ASSERT(self);

	if(keycode == VKK_KEYCODE_ESCAPE)
	{
		// double tap back to exit
		double t1 = cc_timestamp();
		if((t1 - self->escape_t0) < 0.5)
		{
			vkk_engine_platformCmd(self->engine,
			                       VKK_PLATFORM_CMD_EXIT, NULL);
		}
		else
		{
			self->escape_t0 = t1;
		}
	}
	else if(keycode == '=')
	{
		self->exposure *= 1.1;
	}
	else if(keycode == '-')
	{
		self->exposure /= 1.1;
	}
	else if(keycode == '1')
	{
		atmo_renderer_setView(self, 9000.0f, 1.47f, 0.0f,
		                      1.3f, 3.0f, 10.0f);
	}
	else if(keycode == '2')
	{
		atmo_renderer_setView(self, 9000.0f, 1.47f, 0.0f,
		                      1.564f, -3.0f, 10.0f);
	}
	else if(keycode == '3')
	{
		atmo_renderer_setView(self, 7000.0f, 1.57f, 0.0f,
		                      1.54f, -2.96f, 10.0f);
	}
	else if(keycode == '4')
	{
		atmo_renderer_setView(self, 7000.0f, 1.57f, 0.0f,
		                      1.328f, -3.044f, 10.0f);
	}
	else if(keycode == '5')
	{
		atmo_renderer_setView(self, 9000.0f, 1.39f, 0.0f,
		                      1.2f, 0.7f, 10.0f);
	}
	else if(keycode == '6')
	{
		atmo_renderer_setView(self, 9000.0f, 1.5f, 0.0f,
		                      1.628f, 1.05f, 200.0f);
	}
	else if(keycode == '7')
	{
		atmo_renderer_setView(self, 7000.0f, 1.43f, 0.0f,
		                      1.57f, 1.34f, 40.0f);
	}
	else if(keycode == '8')
	{
		atmo_renderer_setView(self, 2.7e6f, 0.81f, 0.0f,
		                      1.57f, 2.0f, 10.0f);
	}
	else if(keycode == '9')
	{
		atmo_renderer_setView(self, 1.2e7f, 0.0f, 0.0f,
		                      0.93f, -2.0f, 10.0f);
	}
	else if(keycode == 'i')
	{
		self->view_distance /= 1.05f;
	}
	else if(keycode == 'o')
	{
		self->view_distance *= 1.05f;
	}
	else if(keycode == 'v')
	{
		self->touch_mode = ATMO_TOUCH_MODE_VIEW;
	}
	else if(keycode == 's')
	{
		self->touch_mode = ATMO_TOUCH_MODE_SUN;
	}
	else if(keycode == 'm')
	{
		self->simple = 1 - self->simple;
	}
}

void atmo_renderer_contentRect(atmo_renderer_t* self,
                               uint32_t t, uint32_t l,
                               uint32_t b, uint32_t r)
{
	ASSERT(self);

	self->content_rect_top    = t;
	self->content_rect_left   = l;
	self->content_rect_width  = r - l;
	self->content_rect_height = b - t;
}
