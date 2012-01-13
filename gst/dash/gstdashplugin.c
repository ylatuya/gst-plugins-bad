/* GStreamer
 * Copyright (C) 2011 Flumotion S.L. <devteam@flumotion.com>
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstdashplugin.c:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstdashsink.h"

GST_DEBUG_CATEGORY (dash_debug);

static gboolean
dash_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (dash_debug, "dashsink", 0, "dashsink");

  if (!gst_element_register (plugin, "dashsink", GST_RANK_SECONDARY,
          GST_TYPE_DASH_SINK) || FALSE)
    return FALSE;
  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "dash",
    "Dynamic adaptive streaming over HTTP",
    dash_init, VERSION, "LGPL", PACKAGE_NAME, "http://www.gstreamer.org/")
