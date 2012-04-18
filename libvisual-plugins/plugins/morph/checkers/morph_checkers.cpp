/* Libvisual-plugins - Standard plugins for libvisual
 * 
 * Copyright (C) 2004, 2005, 2006 Dennis Smit <ds@nerds-incorporated.org>
 *
 * Authors: Dennis Smit <ds@nerds-incorporated.org>
 *
 * $Id: morph_checkers.c,v 1.16 2006/01/27 20:19:18 synap Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <libvisual/libvisual.h>

using namespace LV;

extern "C" const VisPluginInfo *get_plugin_info (void);

namespace {

typedef struct {
    VisTimer *time;
    int flip;
} CheckerPrivate;

int lv_morph_checkers_init (VisPluginData *plugin);
int lv_morph_checkers_cleanup (VisPluginData *plugin);
int lv_morph_checkers_apply (VisPluginData *plugin, float rate, VisAudio *audio, VisVideo *dest, VisVideo *src1, VisVideo *src2);

} // namespace

VISUAL_PLUGIN_API_VERSION_VALIDATOR


extern "C" const VisPluginInfo *get_plugin_info (void)
{
    static VisMorphPlugin morph;
    morph.apply = lv_morph_checkers_apply;
    morph.vidoptions.depth =
            VISUAL_VIDEO_DEPTH_8BIT |
            VISUAL_VIDEO_DEPTH_16BIT |
            VISUAL_VIDEO_DEPTH_24BIT |
            VISUAL_VIDEO_DEPTH_32BIT;

    static VisPluginInfo info;
    info.type = VISUAL_PLUGIN_TYPE_MORPH;

    info.plugname = "checkers";
    info.name = "Checkerboard morph";
    info.author = "Scott Sibley <sisibley@gmail.com>";
    info.version = "0.1";
    info.about = "A checkers in/out morph plugin";
    info.help = "This morph plugin morphs with a checkerboard effect..";
    info.license = VISUAL_PLUGIN_LICENSE_LGPL;
    info.init = lv_morph_checkers_init;
    info.cleanup = lv_morph_checkers_cleanup;
    info.plugin = VISUAL_OBJECT (&morph);

    return &info;
}

namespace {
int lv_morph_checkers_init (VisPluginData *plugin)
{
    CheckerPrivate *priv;

    priv = visual_mem_new0 (CheckerPrivate, 1);
    visual_object_set_private (VISUAL_OBJECT (plugin), priv);

    priv->time = visual_timer_new();

    visual_timer_start(priv->time);

    return 0;
}

int lv_morph_checkers_cleanup (VisPluginData *plugin)
{
    CheckerPrivate *priv = (CheckerPrivate *)visual_object_get_private (VISUAL_OBJECT (plugin));

    visual_mem_free (priv);

    return 0;
}

int lv_morph_checkers_apply (VisPluginData *plugin, float rate, VisAudio *audio, VisVideo *dest, VisVideo *src1, VisVideo *src2)
{
    CheckerPrivate *priv = (CheckerPrivate *)visual_object_get_private (VISUAL_OBJECT (plugin));
    uint8_t *destbuf = (uint8_t *)visual_video_get_pixels (dest);

    if(visual_timer_elapsed_msecs(priv->time) > 300)
    {
        priv->flip++;
        visual_timer_reset(priv->time);
        visual_timer_start(priv->time);
    }

    visual_mem_set (destbuf, 0, visual_video_get_size (dest));
    
    int flip = priv->flip;
    int col, row;
    int x, y;
    int size = dest->width / 5.0;
    VisRectangle *drect = visual_rectangle_new(0, 0, size, size);
    VisRectangle *srect = visual_rectangle_new(0, 0, size, size);
    VisVideo *inter;
    VisVideo *sub = visual_video_new();
    visual_video_copy_attrs(sub, dest);
    visual_video_set_dimension(sub, size, size);
    int32_t data[size * size * dest->bpp];
    visual_video_set_buffer(sub, data); 

    for(col = 0, x = 0; col < dest->width; col+=size, x++)
    {
        for(row = 0, y = 0; row < dest->height; row+=size, y++)
        {
            inter = (row + col + flip) & 1 ? src1 : src2;

            visual_rectangle_set(srect, col, row, size, size);
            visual_video_region_sub(sub, inter, srect);

            visual_video_blit_overlay_rectangle(dest, srect, sub, drect, FALSE);
        }
    }

    visual_video_free_buffer(sub);
    visual_rectangle_free(srect);
    visual_rectangle_free(drect);

    return 0;
}
} // namespace
