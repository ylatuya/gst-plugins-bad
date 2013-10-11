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

#ifndef __GST_JNI_SURFACE_TEXTURE_H__
#define __GST_JNI_SURFACE_TEXTURE_H__

#include <glib.h>
#include <glib-object.h>
#include <jni.h>

G_BEGIN_DECLS

#define GST_TYPE_JNI_SURFACE_TEXTURE                  (gst_jni_surface_texture_get_type ())
#define GST_JNI_SURFACE_TEXTURE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_JNI_SURFACE_TEXTURE, GstJniSurfaceTexture))
#define GST_IS_JNI_SURFACE_TEXTURE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_JNI_SURFACE_TEXTURE))
#define GST_JNI_SURFACE_TEXTURE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_JNI_SURFACE_TEXTURE, GstJniSurfaceTextureClass))
#define GST_IS_JNI_SURFACE_TEXTURE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_JNI_SURFACE_TEXTURE))
#define GST_JNI_SURFACE_TEXTURE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_JNI_SURFACE_TEXTURE, GstJniSurfaceTextureClass))

typedef struct _GstJniSurfaceTexture        GstJniSurfaceTexture;
typedef struct _GstJniSurfaceTextureClass   GstJniSurfaceTextureClass;

struct _GstJniSurfaceTexture
{
  GObject parent_instance;

  /* instance members */
  gint texture_id;
  jobject jobject;
};

struct _GstJniSurfaceTextureClass
{
  GObjectClass parent_class;

  /* class members */
  gint texture_id;

  jclass jklass;
  jmethodID constructor;
  jmethodID set_on_frame_available_listener;
  jmethodID set_default_buffer_size;
  jmethodID update_tex_image;
  jmethodID detach_from_gl_context;
  jmethodID attach_to_gl_context;
  jmethodID get_transform_matrix;
  jmethodID get_timestamp;
  jmethodID release;
};

GType gst_jni_surface_texture_get_type (void);

GstJniSurfaceTexture * gst_jni_surface_texture_new   (void);

void gst_jni_surface_texture_set_default_buffer_size (GstJniSurfaceTexture *texture,
                                                      gint width, gint height);

void gst_jni_surface_texture_update_tex_image        (GstJniSurfaceTexture *texture);

void gst_jni_surface_texture_detach_from_gl_context  (GstJniSurfaceTexture *texture);

void gst_jni_surface_texture_attach_to_gl_context    (GstJniSurfaceTexture *texture,
                                                      gint index);

void gst_jni_surface_texture_get_transform_matrix    (GstJniSurfaceTexture *texture,
                                                      const gfloat *matrix);

gint64 gst_jni_surface_texture_get_timestamp         (GstJniSurfaceTexture *texture);

void gst_jni_surface_texture_release                 (GstJniSurfaceTexture *texture);

G_END_DECLS
#endif
