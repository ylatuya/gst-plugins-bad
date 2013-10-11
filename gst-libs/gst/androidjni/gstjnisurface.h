/*
 * Copyright (C) 2013, Fluendo S.A.
 *   Author: Andoni Morales <amorales@fluendo.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef __GST_JNI_SURFACE_H__
#define __GST_JNI_SURFACE_H__

#include <glib-object.h>
#include <jni.h>
#include "gstjnisurfacetexture.h"

G_BEGIN_DECLS

#define GST_TYPE_JNI_SURFACE                  (gst_jni_surface_get_type ())
#define GST_JNI_SURFACE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_JNI_SURFACE, GstJniSurface))
#define GST_IS_JNI_SURFACE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_JNI_SURFACE))
#define GST_JNI_SURFACE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_JNI_SURFACE, GstJniSurfaceClass))
#define GST_IS_JNI_SURFACE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_JNI_SURFACE))
#define GST_JNI_SURFACE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_JNI_SURFACE, GstJniSurfaceClass))

typedef struct _GstJniSurface        GstJniSurface;
typedef struct _GstJniSurfaceClass   GstJniSurfaceClass;

struct _GstJniSurface
{
  GObject parent_instance;

  /* instance members */
  jobject jobject;
  GstJniSurfaceTexture *texture;
};

struct _GstJniSurfaceClass
{
  GObjectClass parent_class;

  /* class members */
  jclass jklass;
  jmethodID constructor;
  jmethodID is_valid;
  jmethodID release;
  jmethodID describe_contents;
};

GType gst_jni_surface_get_type (void);

GstJniSurface * gst_jni_surface_new           (GstJniSurfaceTexture *texture);

gboolean gst_jni_surface_is_valid             (GstJniSurface *surface);

void gst_jni_surface_release                  (GstJniSurface *surface);

gint gst_jni_surface_describe_contents        (GstJniSurface *surface);

G_END_DECLS
#endif
