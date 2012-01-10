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
