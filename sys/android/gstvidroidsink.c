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

/* Rationale on OpenGL ES version
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

/* These should be defined per model.
 * Galaxy Nexus:
 */
#define VIDROIDSINK_MAX_FRAME_WIDTH 1280
#define VIDROIDSINK_MAX_FRAME_HEIGHT 720

/* XXX: proly  needs ifdef against EGL_KHR_image */
static PFNEGLCREATEIMAGEKHRPROC my_eglCreateImageKHR;
static PFNEGLDESTROYIMAGEKHRPROC my_eglDestroyImageKHR;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC my_glEGLImageTargetTexture2DOES;
#ifndef HAVE_X11
static PFNEGLUNLOCKSURFACEKHRPROC my_eglUnlockSurfaceKHR;
#endif

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

/* XXX: Harcoded for now */
static EGLint vidroidsink_RGB16_config[] = {
  EGL_RED_SIZE, 5,
  EGL_GREEN_SIZE, 6,
  EGL_BLUE_SIZE, 5,
  EGL_NONE
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
static GstFlowReturn gst_vidroidsink_buffer_alloc (GstBaseSink * sink,
    guint64 offset, guint size, GstCaps * caps, GstBuffer ** buf);

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

/* Custom Buffer funcs */
static void gst_vidroidbuffer_destroy (GstViDroidBuffer * vidroidsink);
static void gst_vidroidbuffer_init (GstViDroidBuffer * vidroidsink,
    gpointer g_class);
static GType gst_vidroidbuffer_get_type (void);
static gint gst_vidroidsink_get_compat_format_from_caps
    (GstViDroidSink * vidroidsink, GstCaps * caps);
static void gst_vidroidbuffer_finalize (GstViDroidBuffer * vidroidsink);
static void gst_vidroidbuffer_class_init (gpointer g_class,
    gpointer class_data);
static void gst_vidroidbuffer_free (GstViDroidBuffer * vidroidbuffer);
static GstViDroidBuffer *gst_vidroidbuffer_new (GstViDroidSink * vidroidsink,
    GstCaps * caps);

/* Utility */
static EGLNativeWindowType gst_vidroidsink_create_window (GstViDroidSink *
    vidroidsink, gint width, gint height);
static gboolean gst_vidroidsink_init_egl_display (GstViDroidSink * vidroidsink);
static gboolean gst_vidroidsink_init_egl_surface (GstViDroidSink * vidroidsink);
static void gst_vidroidsink_render_and_display (GstViDroidSink * sink,
    GstBuffer * buf);

static GstBufferClass *gstvidroidsink_buffer_parent_class = NULL;
#define GST_TYPE_VIDROIDBUFFER (gst_vidroidbuffer_get_type())
#define GST_IS_VIDROIDBUFFER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_VIDROIDBUFFER))
#define GST_VIDROIDBUFFER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_VIDROIDBUFFER, GstViDroidBuffer))
#define GST_VIDROIDBUFFER_CAST(obj) ((GstViDroidBuffer *)(obj))


GST_BOILERPLATE_FULL (GstViDroidSink, gst_vidroidsink, GstVideoSink,
    GST_TYPE_VIDEO_SINK, gst_vidroidsink_init_interfaces)

/* Custom Buffer Funcs */
     static GstViDroidBuffer *gst_vidroidbuffer_new (GstViDroidSink *
    vidroidsink, GstCaps * caps)
{
  GstViDroidBuffer *vidroidbuffer = NULL;
  GstStructure *structure = NULL;

  g_return_val_if_fail (GST_IS_VIDROIDSINK (vidroidsink), NULL);
  g_return_val_if_fail (caps, NULL);

  vidroidbuffer =
      (GstViDroidBuffer *) gst_mini_object_new (GST_TYPE_VIDROIDBUFFER);
  GST_DEBUG_OBJECT (vidroidbuffer, "Creating new GstViDroidBuffer");

  structure = gst_caps_get_structure (caps, 0);

  if (!gst_structure_get_int (structure, "width", &vidroidbuffer->width) ||
      !gst_structure_get_int (structure, "height", &vidroidbuffer->height)) {
    GST_WARNING ("failed getting geometry from caps %" GST_PTR_FORMAT, caps);
  }

  GST_LOG_OBJECT (vidroidsink, "creating %dx%d", vidroidbuffer->width,
      vidroidbuffer->height);

  vidroidbuffer->format =
      gst_vidroidsink_get_compat_format_from_caps (vidroidsink, caps);

  if (vidroidbuffer->format == -1) {
    GST_WARNING_OBJECT (vidroidsink,
        "failed to get format from caps %" GST_PTR_FORMAT, caps);
    GST_ERROR_OBJECT (vidroidsink,
        "Invalid input caps. Failed to create  %dx%d buffer",
        vidroidbuffer->width, vidroidbuffer->height);
    goto beach_unlocked;
  }

  vidroidbuffer->vidroidsink = gst_object_ref (vidroidsink);

  vidroidbuffer->image = platform_crate_native_image_buffer
      (vidroidsink->window, vidroidsink->config, vidroidsink->display, NULL);
  if (!vidroidbuffer->image) {
    GST_ERROR_OBJECT (vidroidsink,
        "Failed to create native %sx%d image buffer", vidroidbuffer->width,
        vidroidbuffer->height);
    goto beach_unlocked;
  }

  GST_BUFFER_DATA (vidroidbuffer) = (guchar *) vidroidbuffer->image;
  GST_BUFFER_SIZE (vidroidbuffer) = vidroidbuffer->size;

  return vidroidbuffer;

beach_unlocked:
  gst_vidroidbuffer_free (vidroidbuffer);
  vidroidbuffer = NULL;
  return NULL;
}

static void
gst_vidroidbuffer_destroy (GstViDroidBuffer * vidroidbuffer)
{

  GstViDroidSink *vidroidsink;

  GST_DEBUG_OBJECT (vidroidbuffer, "Destroying buffer");

  vidroidsink = vidroidbuffer->vidroidsink;
  if (G_UNLIKELY (vidroidsink == NULL))
    goto no_sink;

  g_return_if_fail (GST_IS_VIDROIDSINK (vidroidsink));

  GST_OBJECT_LOCK (vidroidsink);
  GST_DEBUG_OBJECT (vidroidsink, "Destroying image");

  if (vidroidbuffer->image) {
    if (GST_BUFFER_DATA (vidroidbuffer)) {
      g_free (GST_BUFFER_DATA (vidroidbuffer));
    }
    vidroidbuffer->image = NULL;
    /* Unallocate EGL/GL especific resources asociated with this
     * Image here
     */
  }

  GST_OBJECT_UNLOCK (vidroidsink);
  vidroidbuffer->vidroidsink = NULL;
  gst_object_unref (vidroidsink);

  GST_MINI_OBJECT_CLASS (gstvidroidsink_buffer_parent_class)->finalize
      (GST_MINI_OBJECT (vidroidbuffer));

  return;

no_sink:
  {
    GST_WARNING ("no sink found");
    return;
  }
}

/* XXX: Missing implementation.
 * This function will have the code for maintaing the pool. readding or
 * destroying the buffers on size or runing/status change. Right now all
 * it does is to call _destroy.
 * for a proper implementation take a look at xvimagesink's image buffer
 * destroy func.
 */
static void
gst_vidroidbuffer_finalize (GstViDroidBuffer * vidroidbuffer)
{
  GstViDroidSink *vidroidsink;

  vidroidsink = vidroidbuffer->vidroidsink;
  if (G_UNLIKELY (vidroidsink == NULL))
    goto no_sink;

  g_return_if_fail (GST_IS_VIDROIDSINK (vidroidsink));

  gst_vidroidbuffer_destroy (vidroidbuffer);

  return;

no_sink:
  {
    GST_WARNING ("no sink found");
    return;
  }
}

static void
gst_vidroidbuffer_free (GstViDroidBuffer * vidroidbuffer)
{
  /* make sure it is not recycled
   * This is meaningless without a pool but was left here
   * as a reference */
  vidroidbuffer->width = -1;
  vidroidbuffer->height = -1;

  gst_buffer_unref (GST_BUFFER (vidroidbuffer));
}

static void
gst_vidroidbuffer_init (GstViDroidBuffer * vidroidsink, gpointer g_class)
{
  return;
}

static void
gst_vidroidbuffer_class_init (gpointer g_class, gpointer class_data)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (g_class);

  gstvidroidsink_buffer_parent_class = g_type_class_peek_parent (g_class);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      gst_vidroidbuffer_finalize;
}

static GType
gst_vidroidbuffer_get_type (void)
{
  static GType _gst_vidroidsink_buffer_type;

  if (G_UNLIKELY (_gst_vidroidsink_buffer_type == 0)) {
    static const GTypeInfo vidroidsink_buffer_info = {
      sizeof (GstBufferClass),
      NULL,
      NULL,
      gst_vidroidbuffer_class_init,
      NULL,
      NULL,
      sizeof (GstViDroidBuffer),
      0,
      (GInstanceInitFunc) gst_vidroidbuffer_init,
      NULL
    };
    _gst_vidroidsink_buffer_type = g_type_register_static (GST_TYPE_BUFFER,
        "GstViDroidBuffer", &vidroidsink_buffer_info, 0);
  }
  return _gst_vidroidsink_buffer_type;
}


/* This function is sort of meaningless right now as we
 * Only Support one image format / caps but was left here
 * as a reference for future improvements. 
 */
static gint
gst_vidroidsink_get_compat_format_from_caps (GstViDroidSink * vidroidsink,
    GstCaps * caps)
{

  GList *list = NULL;
  GstViDroidImageFmt *format;

  g_return_val_if_fail (GST_IS_VIDROIDSINK (vidroidsink), 0);

  list = vidroidsink->supported_fmts;

  /* Traverse the list trying to find a compatible format */
  while (list) {
    format = list->data;
    GST_DEBUG_OBJECT (vidroidsink, "Checking compatibility between listed %"
        GST_PTR_FORMAT " and %" GST_PTR_FORMAT, format->caps, caps);
    if (format) {
      if (gst_caps_can_intersect (caps, format->caps)) {
        return format->fmt;
      }
    }
    list = g_list_next (list);
  }

  return GST_VIDROIDSINK_IMAGE_NOFMT;
}

static GstCaps *
gst_vidroidsink_different_size_suggestion (GstViDroidSink * vidroidsink,
    GstCaps * caps)
{
  GstCaps *intersection;
  GstCaps *new_caps;
  GstStructure *s;
  gint width, height;
  gint par_n = 1, par_d = 1;
  gint dar_n, dar_d;
  gint w, h;

  new_caps = gst_caps_copy (caps);

  s = gst_caps_get_structure (new_caps, 0);

  gst_structure_get_int (s, "width", &width);
  gst_structure_get_int (s, "height", &height);
  gst_structure_get_fraction (s, "pixel-aspect-ratio", &par_n, &par_d);

  gst_structure_remove_field (s, "width");
  gst_structure_remove_field (s, "height");
  gst_structure_remove_field (s, "pixel-aspect-ratio");

  intersection = gst_caps_intersect (vidroidsink->current_caps, new_caps);
  gst_caps_unref (new_caps);

  if (gst_caps_is_empty (intersection))
    return intersection;

  s = gst_caps_get_structure (intersection, 0);

  gst_util_fraction_multiply (width, height, par_n, par_d, &dar_n, &dar_d);

  /* XXX: xvimagesink supports all PARs not sure about our vidroidsink
   * though, need to review this afterwards.
   */

  gst_structure_fixate_field_nearest_int (s, "width", width);
  gst_structure_fixate_field_nearest_int (s, "height", height);
  gst_structure_get_int (s, "width", &w);
  gst_structure_get_int (s, "height", &h);

  gst_util_fraction_multiply (h, w, dar_n, dar_d, &par_n, &par_d);
  gst_structure_set (s, "pixel-aspect-ratio", GST_TYPE_FRACTION, par_n, par_d,
      NULL);

  return intersection;
}

static GstFlowReturn
gst_vidroidsink_buffer_alloc (GstBaseSink * bsink, guint64 offset,
    guint size, GstCaps * caps, GstBuffer ** buf)
{

  GstViDroidSink *vidroidsink;
  GstFlowReturn ret = GST_FLOW_OK;
  GstViDroidBuffer *vidroidbuffer = NULL;
  GstCaps *intersection = NULL;
  GstStructure *structure = NULL;
  GstVideoFormat image_format;
  gint width, height;

  vidroidsink = GST_VIDROIDSINK (bsink);
#ifdef HAVE_X11
  /* no custom alloc for X11 */
  GST_WARNING_OBJECT (vidroidsink, "No custom alloc for x11/mesa");
  *buf = NULL;
  return GST_FLOW_OK;
#endif

  if (G_UNLIKELY (!caps))
    goto no_caps;

  if (G_LIKELY (gst_caps_is_equal (caps, vidroidsink->current_caps))) {
    GST_LOG_OBJECT (vidroidsink,
        "buffer alloc for same last_caps, reusing caps");
    intersection = gst_caps_ref (caps);
    image_format = vidroidsink->format;
    width = GST_VIDEO_SINK_WIDTH (vidroidsink);
    height = GST_VIDEO_SINK_HEIGHT (vidroidsink);

    goto reuse_last_caps;
  }

  GST_DEBUG_OBJECT (vidroidsink, "buffer alloc requested size %d with caps %"
      GST_PTR_FORMAT ", intersecting with our caps %" GST_PTR_FORMAT, size,
      caps, vidroidsink->current_caps);

  /* Check the caps against our current caps */
  intersection = gst_caps_intersect (vidroidsink->current_caps, caps);

  GST_DEBUG_OBJECT (vidroidsink, "intersection in buffer alloc returned %"
      GST_PTR_FORMAT, intersection);

  if (gst_caps_is_empty (intersection)) {
    GstCaps *new_caps;

    gst_caps_unref (intersection);

    /* So we don't support this kind of buffer, let's define one we'd like */
    new_caps = gst_caps_copy (caps);

    structure = gst_caps_get_structure (new_caps, 0);
    if (!gst_structure_has_field (structure, "width") ||
        !gst_structure_has_field (structure, "height")) {
      gst_caps_unref (new_caps);
      goto invalid;
    }

    /* Try different dimensions */
    intersection =
        gst_vidroidsink_different_size_suggestion (vidroidsink, new_caps);

    /* YUV not implemented yet */
    if (gst_caps_is_empty (intersection)) {

      gst_structure_set_name (structure, "video/x-raw-rgb");

      /* Remove format specific fields */
      gst_structure_remove_field (structure, "format");
      gst_structure_remove_field (structure, "endianness");
      gst_structure_remove_field (structure, "depth");
      gst_structure_remove_field (structure, "bpp");
      gst_structure_remove_field (structure, "red_mask");
      gst_structure_remove_field (structure, "green_mask");
      gst_structure_remove_field (structure, "blue_mask");
      gst_structure_remove_field (structure, "alpha_mask");

      /* Reuse intersection with current_caps */
      intersection = gst_caps_intersect (vidroidsink->current_caps, new_caps);
    }

    /* Try with different dimensions */
    if (gst_caps_is_empty (intersection))
      intersection =
          gst_vidroidsink_different_size_suggestion (vidroidsink, new_caps);

    /* Clean this copy */
    gst_caps_unref (new_caps);

    if (gst_caps_is_empty (intersection))
      goto incompatible;
  }

  /* Ensure the returned caps are fixed */
  gst_caps_truncate (intersection);

  GST_DEBUG_OBJECT (vidroidsink, "allocating a buffer with caps %"
      GST_PTR_FORMAT, intersection);
  if (gst_caps_is_equal (intersection, caps)) {
    /* Things work better if we return a buffer with the same caps ptr
     * as was asked for when we can */
    gst_caps_replace (&intersection, caps);
  }

  /* Get image format from caps */
  image_format = gst_vidroidsink_get_compat_format_from_caps (vidroidsink,
      intersection);

  if (image_format == GST_VIDROIDSINK_IMAGE_NOFMT)
    GST_WARNING_OBJECT (vidroidsink, "Can't get a compatible format from caps");

  /* Get geometry from caps */
  structure = gst_caps_get_structure (intersection, 0);
  if (!gst_structure_get_int (structure, "width", &width) ||
      !gst_structure_get_int (structure, "height", &height) ||
      image_format == GST_VIDROIDSINK_IMAGE_NOFMT)
    goto invalid_caps;

reuse_last_caps:

  GST_DEBUG_OBJECT (vidroidsink, "Creating vidroidbuffer");
  vidroidbuffer = gst_vidroidbuffer_new (vidroidsink, intersection);

  if (vidroidbuffer) {
    /* Make sure the buffer is cleared of any previously used flags */
    GST_MINI_OBJECT_CAST (vidroidbuffer)->flags = 0;
    gst_buffer_set_caps (GST_BUFFER_CAST (vidroidbuffer), intersection);
  }

  *buf = GST_BUFFER_CAST (vidroidbuffer);

beach:
  if (intersection) {
    gst_caps_unref (intersection);
  }

  return ret;

  /* ERRORS */
invalid:
  {
    GST_DEBUG_OBJECT (vidroidsink, "No width/hegight on caps!?");
    ret = GST_FLOW_WRONG_STATE;
    goto beach;
  }
incompatible:
  {
    GST_WARNING_OBJECT (vidroidsink, "we were requested a buffer with "
        "caps %" GST_PTR_FORMAT ", but our current caps %" GST_PTR_FORMAT
        " are completely incompatible with those caps", caps,
        vidroidsink->current_caps);
    ret = GST_FLOW_NOT_NEGOTIATED;
    goto beach;
  }
invalid_caps:
  {
    GST_WARNING_OBJECT (vidroidsink, "invalid caps for buffer allocation %"
        GST_PTR_FORMAT, intersection);
    ret = GST_FLOW_NOT_NEGOTIATED;
    goto beach;
  }
no_caps:
  {
    GST_WARNING_OBJECT (vidroidsink, "have no caps, doing fallback allocation");
    *buf = NULL;
    ret = GST_FLOW_OK;
    goto beach;
  }
}

gboolean
gst_vidroidsink_start (GstBaseSink * sink)
{
  gboolean ret;
  GstViDroidSink *vidroidsink = GST_VIDROIDSINK (sink);
  GstViDroidImageFmt *format;

  platform_wrapper_init ();
  g_mutex_lock (vidroidsink->flow_lock);

  /* Init supported caps list (Right now we just harcode the only one we support)
   * XXX: Not sure this is the right place to do it.
   */
  format = g_new0 (GstViDroidImageFmt, 1);
  if (format) {
    format->fmt = GST_VIDROIDSINK_IMAGE_RGB565;
    format->caps = gst_caps_copy (gst_pad_get_pad_template_caps
        (GST_VIDEO_SINK_PAD (vidroidsink)));
    vidroidsink->supported_fmts = g_list_append
        (vidroidsink->supported_fmts, format);
  }

  my_eglCreateImageKHR =
      (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress ("eglCreateImageKHR");
  my_eglDestroyImageKHR =
      (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress ("eglDestroyImageKHR");

  if (!my_eglCreateImageKHR || !my_eglDestroyImageKHR) {
    GST_ERROR_OBJECT (vidroidsink, "Extension EGL_KHR_IMAGE not available");
    goto HANDLE_ERROR;
  }
#ifndef HAVE_X11
  my_eglUnlockSurfaceKHR =
      (PFNEGLUNLOCKSURFACEKHRPROC) eglGetProcAddress ("eglUnlockSurfaceKHR");

  if (!my_eglUnlockSurfaceKHR) {
    GST_ERROR_OBJECT (vidroidsink,
        "Extension eglUnlockSurfaceKHR not available");
    goto HANDLE_ERROR;
  }
#endif

  my_glEGLImageTargetTexture2DOES =
      (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress
      ("glEGLImageTargetTexture2DOES");

  if (!my_glEGLImageTargetTexture2DOES) {
    GST_ERROR_OBJECT (vidroidsink,
        "glEGLImageTargetTexture2DOES not available");
    goto HANDLE_ERROR;
  }

  ret = gst_vidroidsink_init_egl_display (vidroidsink);

  /* XXX: non-NULL from getprocaddress doesn't
   * imply func is supported at runtime. Should check
   * for needed extensions with  glGetString(GL_EXTENSIONS)
   * or reglQueryString(display, EGL_EXTENSIONS) here too. 
   */

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

  if (!width || !height) {      /* Create a default size window */
    width = vidroidsink->window_default_width;
    height = vidroidsink->window_default_height;
  }

  window = platform_create_native_window (width, height);
  if (!window) {
    GST_ERROR_OBJECT (vidroidsink, "Could not create window");
    return window;
  }
  gst_x_overlay_got_window_handle (GST_X_OVERLAY (vidroidsink), window);
  return window;
}

/* XXX: Should implement (redisplay)
 * We need at least the last buffer stored for this to work
 */
static void
gst_vidroidsink_expose (GstXOverlay * overlay)
{
  GstViDroidSink *vidroidsink;
  vidroidsink = GST_VIDROIDSINK (overlay);
  GST_DEBUG_OBJECT (vidroidsink, "Expose catched, redisplay");

  /* Logic would be to get _render_and_display() to use
   * last seen buffer to render from when NULL it's
   * passed on */
  g_mutex_lock (vidroidsink->flow_lock);
  gst_vidroidsink_render_and_display (vidroidsink, NULL);
  g_mutex_unlock (vidroidsink->flow_lock);

  return;
}

static gboolean
gst_vidroidsink_init_egl_surface (GstViDroidSink * vidroidsink)
{
  GST_DEBUG_OBJECT (vidroidsink, "Enter EGL surface setup");

  /* XXX: Impossible?. Check logic and remove if not needed */
  if (G_UNLIKELY (!vidroidsink->have_window)) {
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
  GST_DEBUG_OBJECT (vidroidsink, "Available EGL extensions: %s",
      eglQueryString (vidroidsink->display, EGL_EXTENSIONS));

  /* XXX: Check for vidroidsink's EGL needed versions */

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

  GST_DEBUG_OBJECT (vidroidsink, "EGL Context: %x", vidroidsink->context);

  glEnable (GL_TEXTURE_2D);
  glGenTextures (1, &vidroidsink->textid);
  glBindTexture (GL_TEXTURE_2D, vidroidsink->textid);

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

  if (!id) {
    /* We are being requested to create our own window */
    GST_WARNING_OBJECT (vidroidsink, "OH NOES they want a new window");
    /* 0x0 fires default size creation */
    vidroidsink->window = gst_vidroidsink_create_window (vidroidsink, 0, 0);
    if (!vidroidsink->window) {
      GST_ERROR_OBJECT (vidroidsink, "Got a NULL window");
      goto HANDLE_ERROR;
    }
  } else if (vidroidsink->window == id) {       /* Already used window */
    GST_WARNING_OBJECT (vidroidsink,
        "We've got the same %x window handle again", id);
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

/* Rendering and display */
static void
gst_vidroidsink_render_and_display (GstViDroidSink * vidroidsink,
    GstBuffer * buf)
{
  EGLImageKHR img = EGL_NO_IMAGE_KHR;
  EGLint attrs[] = { EGL_IMAGE_PRESERVED_KHR,
    EGL_FALSE, EGL_NONE, EGL_NONE
  };

  if (!buf) {
    GST_ERROR_OBJECT (vidroidsink, "Null buffer, no past queue implemented");
    goto HANDLE_ERROR;
  }

  img = my_eglCreateImageKHR (vidroidsink->display, EGL_NO_CONTEXT,
      EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer) GST_BUFFER_DATA (buf), attrs);

  if (img == EGL_NO_IMAGE_KHR) {
    GST_ERROR_OBJECT (vidroidsink, "my_eglCreateImageKHR failed");
    goto HANDLE_EGL_ERROR;
  }

  my_glEGLImageTargetTexture2DOES (GL_TEXTURE_2D, img);

HANDLE_EGL_ERROR:
  GST_ERROR_OBJECT (vidroidsink, "EGL call returned error %x", eglGetError ());
HANDLE_ERROR:
  GST_ERROR_OBJECT (vidroidsink, "Rendering disabled for this frame");
}

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

  if (!vidroidsink->surface_ready) {
    GST_ERROR_OBJECT (vidroidsink, "I don't have a surface to render to");
    return GST_FLOW_ERROR;
  }
#ifndef EGL_ANDROID_image_native_buffer
  GST_WARNING_OBJECT (vidroidsink, "EGL_ANDROID_image_native_buffer not "
      "available");
#endif

  g_mutex_lock (vidroidsink->flow_lock);
  gst_vidroidsink_render_and_display (vidroidsink, buf);
  g_mutex_unlock (vidroidsink->flow_lock);

  return GST_FLOW_OK;
}

static gboolean
gst_vidroidsink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
  GstViDroidSink *vidroidsink;
  gboolean ret = TRUE;
  gint width, height;

  vidroidsink = GST_VIDROIDSINK (bsink);

  GST_DEBUG_OBJECT (vidroidsink,
      "In setcaps. Possible caps %" GST_PTR_FORMAT ", setting caps %"
      GST_PTR_FORMAT, vidroidsink->current_caps, caps);

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

  if (gst_vidroidsink_get_compat_format_from_caps (vidroidsink, caps) ==
      GST_VIDROIDSINK_IMAGE_NOFMT) {
    GST_ERROR_OBJECT (vidroidsink, "Unsupported format");
    return FALSE;
  }

  /* XXX: Renegotiation not implemented yet */
  if (vidroidsink->current_caps) {
    GST_ERROR_OBJECT (vidroidsink, "Caps already set. Won't do it again");
    if (gst_caps_is_always_compatible (caps, vidroidsink->current_caps)) {
      GST_INFO_OBJECT (vidroidsink, "Caps are compatible anyway");
      return TRUE;
    } else {
      GST_INFO_OBJECT (vidroidsink,
          "Caps %" GST_PTR_FORMAT "Not always compatible with current-caps %"
          GST_PTR_FORMAT, caps, vidroidsink->current_caps);
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
    GST_INFO_OBJECT (vidroidsink,
        "No window. Will attempt internal window creation");
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

  vidroidsink->current_caps = gst_caps_ref (caps);
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
  GstBaseSinkClass *gstbasesink_class;
  GstVideoSinkClass *gstvideosink_class;

  gobject_class = (GObjectClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  gstvideosink_class = (GstVideoSinkClass *) klass;

  gobject_class->set_property = gst_vidroidsink_set_property;
  gobject_class->get_property = gst_vidroidsink_get_property;

  gstbasesink_class->start = gst_vidroidsink_start;
  gstbasesink_class->stop = gst_vidroidsink_stop;
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_vidroidsink_setcaps);
  gstbasesink_class->buffer_alloc = GST_DEBUG_FUNCPTR
      (gst_vidroidsink_buffer_alloc);

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
          "Default width for self created windows", 0,
          VIDROIDSINK_MAX_FRAME_WIDTH, VIDROIDSINK_MAX_FRAME_WIDTH,
          G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_DEFAULT_HEIGHT,
      g_param_spec_int ("window_default_height", "CanCreateWindow",
          "Default height for self created windows", 0,
          VIDROIDSINK_MAX_FRAME_HEIGHT, VIDROIDSINK_MAX_FRAME_HEIGHT,
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
