/*
 * GStreamer Android EGL/GLES Sink
 * Copyright (C) 2012 Collabora Ltd.
 *   @author: Reynaldo H. Verdejo Pinochet <reynaldo@collabora.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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

/**
 * SECTION:element-vidroidsink
 *
 * GStreamer vout sink using GLES/EGL
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! vidroidsink ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>
#include <gst/interfaces/xoverlay.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "video_platform_wrapper.h"

#include "gstvidroidsink.h"

GST_DEBUG_CATEGORY_STATIC (gst_vidroidsink_debug);
#define GST_CAT_DEFAULT gst_vidroidsink_debug

/* These should be defined per model
 * or proly find a less braindamaged
 * solution. For the Galaxy Nexus we
 * have:
 */
#define MAX_FRAME_WIDTH 1280
#define MAX_FRAME_HEIGHT 720

/* XXX: proly  needs ifdef against EGL_KHR_image */
static PFNEGLCREATEIMAGEKHRPROC egl_ext_create_image;
static PFNEGLDESTROYIMAGEKHRPROC egl_ext_destroy_image;

/* Input capabilities.
 *
 * OpenGL ES Standard does not mandate YUV support
 *
 * XXX: Extend RGB support to a set
 * XXX: YUV seems to be supported by our primary target at least..
 */
static GstStaticPadTemplate gst_vidroidsink_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_RGB_16));

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT,
  PROP_CAN_CREATE_WINDOW,
  PROP_DEFAULT_HEIGHT,
  PROP_DEFAULT_WIDTH
};

static void gst_vidroidsink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_vidroidsink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static GstFlowReturn gst_vidroidsink_show_frame (GstVideoSink * vsink,
    GstBuffer * buf);
static gboolean gst_vidroidsink_setcaps (GstBaseSink * bsink, GstCaps * caps);
static gboolean gst_vidroidsink_start (GstBaseSink * sink);
static gboolean gst_vidroidsink_stop (GstBaseSink * sink);

/* XOverlay interface cruft */
static gboolean gst_vidroidsink_interface_supported
    (GstImplementsInterface * iface, GType type);
static void gst_vidroidsink_implements_init
    (GstImplementsInterfaceClass * klass);
static void gst_vidroidsink_xoverlay_init (GstXOverlayClass * iface);
static void gst_vidroidsink_init_interfaces (GType type);

/* Actual XOverlay interface funcs */
static void gst_vidroidsink_expose (GstXOverlay * overlay);
static void gst_vidroidsink_set_window_handle (GstXOverlay * overlay,
    guintptr id);

/* Utility */
static EGLNativeWindowType gst_vidroidsink_create_window (GstViDroidSink *
    vidroidsink, gint width, gint height);
static gboolean gst_vidroidsink_init_egl_display (GstViDroidSink * vidroidsink);
static gboolean gst_vidroidsink_init_egl_surface (GstViDroidSink * vidroidsink);

GST_BOILERPLATE_FULL (GstViDroidSink, gst_vidroidsink, GstVideoSink,
    GST_TYPE_VIDEO_SINK, gst_vidroidsink_init_interfaces)

/* End prototypes */
    gboolean
gst_vidroidsink_start (GstBaseSink * sink)
{
  gboolean ret;
  GstViDroidSink *vidroidsink = GST_VIDROIDSINK (sink);

  platform_wrapper_init ();
  g_mutex_lock (vidroidsink->flow_lock);

  /* XXX: non-NULL from getprocaddress doesn't
   * imply func is supported at runtime. Should check
   * with  glGetString(GL_EXTENSIONS) o
   * reglQueryString(display, EGL_EXTENSIONS) too
   */
  egl_ext_create_image =
      (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress ("eglCreateImageKHR");
  egl_ext_destroy_image =
      (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress ("eglDestroyImageKHR");

  if (!egl_ext_create_image || !egl_ext_destroy_image) {
    GST_ERROR_OBJECT (vidroidsink, "EGL_KHR_IMAGE ext not available");
    goto HANDLE_ERROR;
  }

  ret = gst_vidroidsink_init_egl_display (vidroidsink);
  g_mutex_unlock (vidroidsink->flow_lock);

  if (!ret) {
    GST_ERROR_OBJECT (vidroidsink, "Couldn't init EGL display. Bailing out");
    goto HANDLE_ERROR;
  }

  return TRUE;

HANDLE_ERROR:
  g_mutex_unlock (vidroidsink->flow_lock);
  return FALSE;
}

/* XXX: Should implement */
gboolean
gst_vidroidsink_stop (GstBaseSink * sink)
{
  return TRUE;
}

static void
gst_vidroidsink_xoverlay_init (GstXOverlayClass * iface)
{
  iface->set_window_handle = gst_vidroidsink_set_window_handle;
  iface->expose = gst_vidroidsink_expose;
}

static gboolean
gst_vidroidsink_interface_supported (GstImplementsInterface * iface, GType type)
{
  return (type == GST_TYPE_X_OVERLAY);
}

static void
gst_vidroidsink_implements_init (GstImplementsInterfaceClass * klass)
{
  klass->supported = gst_vidroidsink_interface_supported;
}

/* XXX: Review. Drafted */
static EGLNativeWindowType
gst_vidroidsink_create_window (GstViDroidSink * vidroidsink, gint width,
    gint height)
{
  EGLNativeWindowType window;

  window = platform_create_native_window (width, height);
  if (!window) {
    GST_ERROR_OBJECT (vidroidsink, "Could not create window");
    return window;
  }
  gst_x_overlay_got_window_handle (GST_X_OVERLAY (vidroidsink), window);
  return window;
}

/* XXX: Should implement (redisplay) */
static void
gst_vidroidsink_expose (GstXOverlay * overlay)
{
  /* Sample code from glimagesink
     GstGLImageSink *glimage_sink = GST_GLIMAGE_SINK (overlay);

     //redisplay opengl scene
     if (glimage_sink->display && glimage_sink->window_id) {

     if (glimage_sink->window_id != glimage_sink->new_window_id) {
     glimage_sink->window_id = glimage_sink->new_window_id;
     gst_gl_display_set_window_id (glimage_sink->display,
     glimage_sink->window_id);
     }

     gst_gl_display_redisplay (glimage_sink->display, 0, 0, 0, 0, 0,
     glimage_sink->keep_aspect_ratio);
     }
   */
}

static gboolean
gst_vidroidsink_init_egl_surface (GstViDroidSink * vidroidsink)
{
  GST_DEBUG_OBJECT (vidroidsink, "Enter EGL surface setup");

  /* XXX: Unlikely. Check logic and remove if not needed */
  if (!vidroidsink->have_window) {
    GST_ERROR_OBJECT (vidroidsink, "Attempted to setup surface without window");
    goto HANDLE_ERROR;
  }

  vidroidsink->surface = eglCreateWindowSurface (vidroidsink->display,
      vidroidsink->config, vidroidsink->window, NULL);

  if (vidroidsink->surface == EGL_NO_SURFACE) {
    GST_ERROR_OBJECT (vidroidsink, "Can't create surface, eglCreateSurface");
    goto HANDLE_EGL_ERROR;
  }

  if (!eglMakeCurrent (vidroidsink->display, vidroidsink->surface,
          vidroidsink->surface, vidroidsink->context)) {
    GST_ERROR_OBJECT (vidroidsink, "Couldn't bind surface/context, "
        "eglMakeCurrent");
    goto HANDLE_EGL_ERROR;
  }

  /* Ship it! */
  vidroidsink->surface_ready = TRUE;
  return TRUE;

  /* Errors */
HANDLE_EGL_ERROR:
  GST_ERROR_OBJECT (vidroidsink, "EGL call returned error %x", eglGetError ());
HANDLE_ERROR:
  GST_ERROR_OBJECT (vidroidsink, "Couldn't setup EGL surface");
  g_mutex_unlock (vidroidsink->flow_lock);
  return FALSE;
}

static gboolean
gst_vidroidsink_init_egl_display (GstViDroidSink * vidroidsink)
{
  GLint egl_configs;

  GST_DEBUG_OBJECT (vidroidsink, "Enter EGL initial configuration");

  /* Begin real action! BIG_FUCKING_WARNING: This needs to be reviewed */

  vidroidsink->display = eglGetDisplay (EGL_DEFAULT_DISPLAY);
  if (vidroidsink->display == EGL_NO_DISPLAY) {
    GST_ERROR_OBJECT (vidroidsink, "Could not get EGL display connection");
    goto HANDLE_ERROR;          /* No EGL error is set by eglGetDisplay() */
  }

  if (eglInitialize (vidroidsink->display, &vidroidsink->majorVersion,
          &vidroidsink->minorVersion) == EGL_FALSE) {
    GST_ERROR_OBJECT (vidroidsink, "Could not init EGL display connection");
    goto HANDLE_EGL_ERROR;
  }

  GST_DEBUG_OBJECT (vidroidsink, "EGL version %d.%d", vidroidsink->majorVersion,
      vidroidsink->minorVersion);

  /* XXX: Check for vidroidsink's EGL supported versions */

  /* XXX: this doesn't work, check how to get window dimensions!
     GST_INFO_OBJECT (vidroidsink, "Window: %d*%d format=%d\n",
     vidroidsink->window->width,
     vidroidsink->window->height,
     vidroidsink->window->format);
   */

  if (!eglChooseConfig (vidroidsink->display, vidroidsink_RGB16_config,
          &vidroidsink->config, 1, &egl_configs)) {
    GST_ERROR_OBJECT (vidroidsink, "eglChooseConfig failed\n");
    goto HANDLE_ERROR;
  }

  /* XXX: Should really attempt tp create a new one or ...
   * vidroidsink->context = eglGetCurrentContext() ?
   */
  vidroidsink->context = eglCreateContext (vidroidsink->display,
      vidroidsink->config, EGL_NO_CONTEXT, NULL);

  if (vidroidsink->context == EGL_NO_CONTEXT) {
    GST_ERROR_OBJECT (vidroidsink, "Error getting context, eglCreateContext");
    goto HANDLE_EGL_ERROR;
  }

  GST_INFO_OBJECT (vidroidsink, "GL context: %x", vidroidsink->context);


  /* XXX: Not sure this belongs here
   * Enable 2D texturing */
  glEnable (GL_TEXTURE_2D);

  return TRUE;

  /* Errors */
HANDLE_EGL_ERROR:
  GST_ERROR_OBJECT (vidroidsink, "EGL call returned error %x", eglGetError ());
HANDLE_ERROR:
  GST_ERROR_OBJECT (vidroidsink, "Couldn't setup window/surface from handle");
  return FALSE;
}

/* XXX: Never actually tested */
static void
gst_vidroidsink_set_window_handle (GstXOverlay * overlay, guintptr id)
{
  GstViDroidSink *vidroidsink = GST_VIDROIDSINK (overlay);

  g_return_if_fail (GST_IS_VIDROIDSINK (vidroidsink));
  GST_DEBUG_OBJECT (vidroidsink, "We got a window handle!");

  g_mutex_lock (vidroidsink->flow_lock);

  /* If !id we are being requested to create a window to display on.
   * This is no yet implemented in the code but it's here as a reference
   */
  if (!id) {
    GST_WARNING_OBJECT (vidroidsink, "OH NOES they want a new window");
    /* XXX: 0,0 should fire default size creation on _create_window() */
    vidroidsink->window = gst_vidroidsink_create_window (vidroidsink, 0, 0);
    if (!vidroidsink->window) {
      GST_ERROR_OBJECT (vidroidsink, "Got a NULL window");
      goto HANDLE_ERROR;
    }
  } else if (vidroidsink->window == id) {
    /* Already used window? (under what circumstances this can happen??) */
    GST_WARNING_OBJECT (vidroidsink, "We've got the %x handle again", id);
    GST_INFO_OBJECT (vidroidsink, "Skipping surface setup");
    goto HANDLE_ERROR;
  } else {
    vidroidsink->window = (EGLNativeWindowType) id;
  }

  /* OK, we have a new window */
  vidroidsink->have_window = TRUE;
  if (!gst_vidroidsink_init_egl_surface (vidroidsink)) {
    GST_ERROR_OBJECT (vidroidsink, "Couldn't init EGL surface!");
    goto HANDLE_ERROR;
  }

  g_mutex_unlock (vidroidsink->flow_lock);
  return;

  /* Errors */
HANDLE_ERROR:
  GST_ERROR_OBJECT (vidroidsink, "Couldn't setup window/surface from handle");
  g_mutex_unlock (vidroidsink->flow_lock);
  return;
}

/* Rendering & display
 *
 * Rationale on OpenGL ES version
 *
 * So we have a window/surface/display/context
 * now we need to render and display.
 * Android supports OpenGL ES 1.0 / 1.1 and since 2.2
 * (API level 8) it supports OpenGL ES 2.0. Most widely
 * supported version is OpenGL ES 1.1 according to
 * http://developer.android.com/resources/dashboard/opengl.html
 * and its both easier to use and more suitable for the
 * relative low complexity of what we are trying to achive,
 * so that's what we will use
 */
static GstFlowReturn
gst_vidroidsink_show_frame (GstVideoSink * vsink, GstBuffer * buf)
{
  GstViDroidSink *vidroidsink;

  g_return_val_if_fail (buf != NULL, GST_FLOW_ERROR);

  vidroidsink = GST_VIDROIDSINK (vsink);
  GST_DEBUG_OBJECT (vidroidsink, "Got buffer: %p", buf);

  if (!vidroidsink->have_window) {
    GST_ERROR_OBJECT (vidroidsink, "I don't have a window to render to");
    return GST_FLOW_ERROR;
  }
#ifdef EGL_ANDROID_image_native_buffer

  /* Real deal */

#else
  GST_WARNING_OBJECT (vidroidsink, "EGL_ANDROID_image_native_buffer not "
      "available");
#endif
  return GST_FLOW_OK;
}

static gboolean
gst_vidroidsink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
  GstViDroidSink *vidroidsink;
  gboolean ret = TRUE;
  //GstStructure *capstruct;
  gint width, height;

  vidroidsink = GST_VIDROIDSINK (bsink);

  GST_DEBUG_OBJECT (vidroidsink, "Got caps %", caps);

  //capstruct = gst_caps_get_structure (caps, 0);
  //  ie: gst_structure_get_value (capstruct, "framerate");

  /* Quick safe measures */
  if (!(ret = gst_video_format_parse_caps (caps, &vidroidsink->format, &width,
              &height))) {
    GST_ERROR_OBJECT (vidroidsink, "Got weird and/or incomplete caps");
    return FALSE;
  }

  if (vidroidsink->format == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_ERROR_OBJECT (vidroidsink, "Got unknown video format caps");
    return FALSE;
  }

  /* XXX: Renegotiation not implemented yet */
  if (vidroidsink->current_caps) {
    GST_ERROR_OBJECT (vidroidsink, "Caps already set, won't do it again");
    if (gst_caps_is_always_compatible (vidroidsink->current_caps, caps)) {
      GST_INFO_OBJECT (vidroidsink, "Caps are compatible anyway..");
      return TRUE;
    } else {
      GST_WARNING_OBJECT (vidroidsink, "Renegotiation not implemented");
      return FALSE;
    }
  }

  /* OK, got caps and have none. Should be the first time!
   * Write on our diary! A time to remember!.. And ask
   * application to prety please give us a window while we
   * are at it.
   */
  g_mutex_lock (vidroidsink->flow_lock);
  if (!vidroidsink->have_window) {
    g_mutex_unlock (vidroidsink->flow_lock);
    gst_x_overlay_prepare_xwindow_id (GST_X_OVERLAY (vidroidsink));
  } else {
    g_mutex_unlock (vidroidsink->flow_lock);
  }

  GST_VIDEO_SINK_WIDTH (vidroidsink) = width;
  GST_VIDEO_SINK_HEIGHT (vidroidsink) = height;

  g_mutex_lock (vidroidsink->flow_lock);
  if (!vidroidsink->have_window) {
    if (!(vidroidsink->window = gst_vidroidsink_create_window (vidroidsink,
                width, height))) {
      GST_ERROR_OBJECT (vidroidsink, "Internal window creation failed!");
      g_mutex_unlock (vidroidsink->flow_lock);
      return FALSE;
    }
  }

  vidroidsink->have_window = TRUE;
  vidroidsink->current_caps = gst_caps_ref (caps);

  if (!gst_vidroidsink_init_egl_surface (vidroidsink)) {
    g_mutex_unlock (vidroidsink->flow_lock);
    GST_ERROR_OBJECT (vidroidsink, "Couldn't init EGL surface from window");
    return FALSE;
  }

  vidroidsink->surface_ready = TRUE;
  g_mutex_unlock (vidroidsink->flow_lock);
  return TRUE;
}

static void
gst_vidroidsink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstViDroidSink *vidroidsink;

  g_return_if_fail (GST_IS_VIDROIDSINK (object));

  vidroidsink = GST_VIDROIDSINK (object);

  switch (prop_id) {
    case PROP_SILENT:
      vidroidsink->silent = g_value_get_boolean (value);
      break;
    case PROP_CAN_CREATE_WINDOW:
      vidroidsink->can_create_window = g_value_get_boolean (value);
      break;
    case PROP_DEFAULT_HEIGHT:
      vidroidsink->window_default_height = g_value_get_int (value);
      break;
    case PROP_DEFAULT_WIDTH:
      vidroidsink->window_default_width = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_vidroidsink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstViDroidSink *vidroidsink;

  g_return_if_fail (GST_IS_VIDROIDSINK (object));

  vidroidsink = GST_VIDROIDSINK (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, vidroidsink->silent);
      break;
    case PROP_CAN_CREATE_WINDOW:
      g_value_set_boolean (value, vidroidsink->can_create_window);
      break;
    case PROP_DEFAULT_HEIGHT:
      g_value_set_int (value, vidroidsink->window_default_height);
      break;
    case PROP_DEFAULT_WIDTH:
      g_value_set_int (value, vidroidsink->window_default_width);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_vidroidsink_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "EGL/GLES Android Sink",
      "Sink/Video",
      "An EGL/GLES Android Video Sink Implementing the XOverlay interface",
      "Reynaldo H. Verdejo Pinochet <reynaldo@collabora.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_vidroidsink_sink_template_factory));
}

/* initialize the vidroidsink's class */
static void
gst_vidroidsink_class_init (GstViDroidSinkClass * klass)
{
  GObjectClass *gobject_class;
//  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;
  GstVideoSinkClass *gstvideosink_class;

  gobject_class = (GObjectClass *) klass;
//  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  gstvideosink_class = (GstVideoSinkClass *) klass;

  gobject_class->set_property = gst_vidroidsink_set_property;
  gobject_class->get_property = gst_vidroidsink_get_property;

  gstbasesink_class->start = gst_vidroidsink_start;
  gstbasesink_class->stop = gst_vidroidsink_stop;
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_vidroidsink_setcaps);

  gstvideosink_class->show_frame =
      GST_DEBUG_FUNCPTR (gst_vidroidsink_show_frame);

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_CAN_CREATE_WINDOW,
      g_param_spec_boolean ("can_create_window", "CanCreateWindow",
          "Attempt to create a window?", FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_DEFAULT_WIDTH,
      g_param_spec_int ("window_default_width", "DefaultWidth",
          "Default width for self created windows", 0, MAX_FRAME_WIDTH, 640,
          G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_DEFAULT_HEIGHT,
      g_param_spec_int ("window_default_height", "CanCreateWindow",
          "Default height for self created windows", 0, MAX_FRAME_HEIGHT, 480,
          G_PARAM_READWRITE));
}


static void
gst_vidroidsink_init (GstViDroidSink * vidroidsink,
    GstViDroidSinkClass * gclass)
{
  /* Init defaults */
  vidroidsink->have_window = FALSE;
  vidroidsink->surface_ready = FALSE;
  vidroidsink->flow_lock = g_mutex_new ();
}

/* Interface initializations. Used here for initializing the XOverlay
 * Interface.
 */
static void
gst_vidroidsink_init_interfaces (GType type)
{
  static const GInterfaceInfo implements_info = {
    (GInterfaceInitFunc) gst_vidroidsink_implements_init, NULL, NULL
  };

  static const GInterfaceInfo xoverlay_info = {
    (GInterfaceInitFunc) gst_vidroidsink_xoverlay_init, NULL, NULL
  };

  g_type_add_interface_static (type, GST_TYPE_IMPLEMENTS_INTERFACE,
      &implements_info);
  g_type_add_interface_static (type, GST_TYPE_X_OVERLAY, &xoverlay_info);

}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
vidroidsink_plugin_init (GstPlugin * plugin)
{
  /* debug category for fltering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_vidroidsink_debug, "vidroidsink",
      0, "Simple EGL/GLES Sink");

  return gst_element_register (plugin, "vidroidsink", GST_RANK_NONE,
      GST_TYPE_VIDROIDSINK);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "EGL/GLES Android Sink"
#endif

#ifndef VERSION
#define VERSION "0.911"
#endif

/* gstreamer looks for this structure to register vidroidsinks
 *
 * exchange the string 'Template vidroidsink' with your vidroidsink description
 */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "vidroidsink",
    "EGL/GLES Android sink",
    vidroidsink_plugin_init,
    VERSION, "LGPL", "GStreamer", "http://gstreamer.net/")
