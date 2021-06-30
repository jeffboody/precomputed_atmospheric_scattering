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

#include <stdlib.h>

#define LOG_TAG "atmo"
#include "libcc/cc_log.h"
#include "libvkk/vkk_platform.h"
#include "atmo_renderer.h"

/***********************************************************
* callbacks                                                *
***********************************************************/

void* demovk_onCreate(vkk_engine_t* engine)
{
	ASSERT(engine);

	return (void*) atmo_renderer_new(engine);
}

void demovk_onDestroy(void** _priv)
{
	ASSERT(_priv);

	atmo_renderer_delete((atmo_renderer_t**) _priv);
}

void demovk_onDraw(void* priv)
{
	ASSERT(priv);

	atmo_renderer_draw((atmo_renderer_t*) priv);
}

void demovk_onPause(void* priv)
{
	ASSERT(priv);

	// ignore
}

void demovk_onEvent(void* priv, vkk_event_t* event)
{
	ASSERT(priv);
	ASSERT(event);

	atmo_renderer_t* renderer = (atmo_renderer_t*) priv;

	if(event->type == VKK_EVENT_TYPE_ACTION_DOWN)
	{
		if(event->action.count != 1)
		{
			return;
		}

		atmo_renderer_touch(renderer,
		                    ATMO_TOUCH_EVENT_DOWN,
		                    event->action.coord[0].x,
		                    event->action.coord[0].y);
	}
	else if(event->type == VKK_EVENT_TYPE_ACTION_MOVE)
	{
		if(event->action.count != 1)
		{
			return;
		}

		atmo_renderer_touch(renderer,
		                    ATMO_TOUCH_EVENT_MOVE,
		                    event->action.coord[0].x,
		                    event->action.coord[0].y);
	}
	else if(event->type == VKK_EVENT_TYPE_ACTION_UP)
	{
		if(event->action.count != 1)
		{
			return;
		}

		atmo_renderer_touch(renderer,
		                    ATMO_TOUCH_EVENT_UP,
		                    event->action.coord[0].x,
		                    event->action.coord[0].y);
	}
	else if(event->type == VKK_EVENT_TYPE_DENSITY)
	{
		atmo_renderer_density(renderer,
		                      event->density);
	}
	else if((event->type == VKK_EVENT_TYPE_KEY_UP) ||
	        ((event->type == VKK_EVENT_TYPE_KEY_DOWN) &&
	         (event->key.repeat)))
	{
		atmo_renderer_keyPress(renderer,
		                       event->key.keycode,
		                       event->key.meta);
	}
	else if(event->type == VKK_EVENT_TYPE_CONTENT_RECT)
	{
		atmo_renderer_contentRect(renderer,
		                          event->content_rect.t,
		                          event->content_rect.l,
		                          event->content_rect.b,
		                          event->content_rect.r);
	}
}

vkk_platformInfo_t VKK_PLATFORM_INFO =
{
	.app_name    = "DemoVK",
	.app_version =
	{
		.major = 1,
		.minor = 0,
		.patch = 1,
	},
	.app_dir     = "DemoVK",
	.onCreate    = demovk_onCreate,
	.onDestroy   = demovk_onDestroy,
	.onPause     = demovk_onPause,
	.onDraw      = demovk_onDraw,
	.onEvent     = demovk_onEvent,
};
