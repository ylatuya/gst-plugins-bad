/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *               2000,2005 Wim Taymans <wim@fluendo.com>
 *
 * gstios_assetsrc.c:
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
 * SECTION:element-ios_assetsrc
 * @see_also: #GstIOSAssetSrc
 *
 * Read data from an iOS asset from the media library.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch iosassetsrc uri=assets-library://asset/asset.M4V?id=11&ext=M4V ! decodebin2 ! autoaudiosink
 * ]| Plays asset with id a song.ogg from local dir.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/gst.h>
#include "iosassetsrc.h"

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_ios_asset_src_debug);
#define GST_CAT_DEFAULT gst_ios_asset_src_debug


#define DEFAULT_BLOCKSIZE       4*1024

enum
{
  ARG_0,
  ARG_URI,
};

static void gst_ios_asset_src_finalize (GObject * object);

static void gst_ios_asset_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ios_asset_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_ios_asset_src_start (GstBaseSrc * basesrc);
static gboolean gst_ios_asset_src_stop (GstBaseSrc * basesrc);

static gboolean gst_ios_asset_src_is_seekable (GstBaseSrc * src);
static gboolean gst_ios_asset_src_get_size (GstBaseSrc * src, guint64 * size);
static GstFlowReturn gst_ios_asset_src_create (GstBaseSrc * src, guint64 offset,
    guint length, GstBuffer ** buffer);
static gboolean gst_ios_asset_src_query (GstBaseSrc * src, GstQuery * query);

static void gst_ios_asset_src_uri_handler_init (gpointer g_iface,
    gpointer iface_data);

static void
_do_init (GType ios_assetsrc_type)
{
  static const GInterfaceInfo urihandler_info = {
    gst_ios_asset_src_uri_handler_init,
    NULL,
    NULL
  };

  g_type_add_interface_static (ios_assetsrc_type, GST_TYPE_URI_HANDLER,
      &urihandler_info);
  GST_DEBUG_CATEGORY_INIT (gst_ios_asset_src_debug, "iosassetsrc", 0, "iosassetsrc element");
}

GST_BOILERPLATE_FULL (GstIOSAssetSrc, gst_ios_asset_src, GstBaseSrc, GST_TYPE_BASE_SRC,
    _do_init);

static void
gst_ios_asset_src_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (gstelement_class,
      "IOSAsset Source",
      "Source/File",
      "Read from arbitrary point in a iOS asset",
      "Andoni Morales Alastruey <amorales@fluendo.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&srctemplate));
}

static void
gst_ios_asset_src_class_init (GstIOSAssetSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstbasesrc_class = GST_BASE_SRC_CLASS (klass);

  gobject_class->set_property = gst_ios_asset_src_set_property;
  gobject_class->get_property = gst_ios_asset_src_get_property;

  g_object_class_install_property (gobject_class, ARG_URI,
      g_param_spec_string ("uri", "Asset URI",
          "URI of the asset to read", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  gobject_class->finalize = gst_ios_asset_src_finalize;

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_ios_asset_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_ios_asset_src_stop);
  gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR (gst_ios_asset_src_is_seekable);
  gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR (gst_ios_asset_src_get_size);
  gstbasesrc_class->create = GST_DEBUG_FUNCPTR (gst_ios_asset_src_create);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR (gst_ios_asset_src_query);
}

static void
gst_ios_asset_src_init (GstIOSAssetSrc * src, GstIOSAssetSrcClass * g_class)
{
  src->uri = NULL;
  src->asset = NULL;
  src->library = [[GstAssetsLibrary alloc] init];
  gst_base_src_set_blocksize (GST_BASE_SRC (src), DEFAULT_BLOCKSIZE);
}

static void
gst_ios_asset_src_free_resources (GstIOSAssetSrc *src)
{
  if (src->asset != NULL) {
    [src->asset release];
    src->asset = NULL;
  }

  if (src->url != NULL) {
    [src->url release];
    src->url = NULL;
  }

  if (src->uri != NULL) {
    g_free (src->uri);
    src->uri = NULL;
  }
}

static void
gst_ios_asset_src_finalize (GObject * object)
{
  GstIOSAssetSrc *src;

  src = GST_IOS_ASSET_SRC (object);
  gst_ios_asset_src_free_resources (src);
  [src->library release];

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_ios_asset_src_set_uri (GstIOSAssetSrc * src, const gchar * uri)
{
  GstState state;
  NSString *nsuristr;
  NSURL *url;

  /* the element must be stopped in order to do this */
  GST_OBJECT_LOCK (src);
  state = GST_STATE (src);
  if (state != GST_STATE_READY && state != GST_STATE_NULL)
    goto wrong_state;
  GST_OBJECT_UNLOCK (src);

  gst_ios_asset_src_free_resources (src);

  nsuristr = [[NSString alloc] initWithUTF8String:uri];
  url = [[NSURL alloc] initWithString:nsuristr];
  [nsuristr release];
 
  if (url == NULL) {
    GST_ERROR_OBJECT (src, "Invalid URI      : %s", src->uri);
    return FALSE;
  }

  GST_INFO_OBJECT (src, "URI      : %s", src->uri);
  src->url = url;
  src->uri = g_strdup (uri);
  g_object_notify (G_OBJECT (src), "uri");
  gst_uri_handler_new_uri (GST_URI_HANDLER (src), src->uri);

  return TRUE;

  /* ERROR */
wrong_state:
  {
    g_warning ("Changing the 'uri' property on iosassetsrc when an asset is "
        "open is not supported.");
    GST_OBJECT_UNLOCK (src);
    return FALSE;
  }
}

static void
gst_ios_asset_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstIOSAssetSrc *src;

  g_return_if_fail (GST_IS_IOS_ASSET_SRC (object));

  src = GST_IOS_ASSET_SRC (object);

  switch (prop_id) {
    case ARG_URI:
      gst_ios_asset_src_set_uri (src, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ios_asset_src_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstIOSAssetSrc *src;

  g_return_if_fail (GST_IS_IOS_ASSET_SRC (object));

  src = GST_IOS_ASSET_SRC (object);

  switch (prop_id) {
    case ARG_URI:
      g_value_set_string (value, src->uri);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_ios_asset_src_create (GstBaseSrc * basesrc, guint64 offset, guint length,
    GstBuffer ** buffer)
{
  GstBuffer *buf;
  NSError *err;
  guint bytes_read;
  GstIOSAssetSrc *src = GST_IOS_ASSET_SRC (basesrc);

  buf = gst_buffer_try_new_and_alloc (length);
  if (G_UNLIKELY (buf == NULL && length > 0)) {
    GST_ERROR_OBJECT (src, "Failed to allocate %u bytes", length);
    return GST_FLOW_ERROR;
  }

  /* No need to read anything if length is 0 */
  GST_BUFFER_SIZE (buf) = 0;
  GST_BUFFER_OFFSET (buf) = offset;
  GST_BUFFER_OFFSET_END (buf) = offset;
  bytes_read = [src->asset getBytes: GST_BUFFER_DATA (buffer)
                           fromOffset:offset
                           length:length
                           error:&err];
  if (G_UNLIKELY (err != NULL)) {
    goto could_not_read;
  }

  /* we should eos if we read less than what was requested */
  if (G_UNLIKELY (bytes_read < length)) {
    goto eos;
  }

  GST_BUFFER_SIZE (buf) = bytes_read;
  GST_BUFFER_OFFSET_END (buf) = offset + bytes_read;

  *buffer = buf;

  return GST_FLOW_OK;

  /* ERROR */
could_not_read:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
eos:
  {
    GST_DEBUG ("EOS");
    gst_buffer_unref (buf);
    return GST_FLOW_UNEXPECTED;
  }
}

static gboolean
gst_ios_asset_src_query (GstBaseSrc * basesrc, GstQuery * query)
{
  gboolean ret = FALSE;
  GstIOSAssetSrc *src = GST_IOS_ASSET_SRC (basesrc);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_URI:
      gst_query_set_uri (query, src->uri);
      ret = TRUE;
      break;
    default:
      ret = FALSE;
      break;
  }

  if (!ret)
    ret = GST_BASE_SRC_CLASS (parent_class)->query (basesrc, query);

  return ret;
}

static gboolean
gst_ios_asset_src_is_seekable (GstBaseSrc * basesrc)
{
  return TRUE;
}

static gboolean
gst_ios_asset_src_get_size (GstBaseSrc * basesrc, guint64 * size)
{
  GstIOSAssetSrc *src;

  src = GST_IOS_ASSET_SRC (basesrc);

  *size = (guint64) [src->asset size];
  return TRUE;
}

static gboolean
gst_ios_asset_src_start (GstBaseSrc * basesrc)
{
  GstIOSAssetSrc *src = GST_IOS_ASSET_SRC (basesrc);

  src->asset = [src->library assetForURLSync: src->url];

  if (src->asset == NULL) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        ("Could not open asset \"%s\" for reading.", src->uri),
        GST_ERROR_SYSTEM);
    return FALSE;
  };

  return TRUE;
}

/* unmap and close the ios_asset */
static gboolean
gst_ios_asset_src_stop (GstBaseSrc * basesrc)
{
  GstIOSAssetSrc *src = GST_IOS_ASSET_SRC (basesrc);

  [src->asset release];
  return TRUE;
}

static GstURIType
gst_ios_asset_src_uri_get_type (void)
{
  return GST_URI_SRC;
}

static gchar **
gst_ios_asset_src_uri_get_protocols (void)
{
  static gchar *protocols[] = { (char *) "assets-library", NULL };

  return protocols;
}

static const gchar *
gst_ios_asset_src_uri_get_uri (GstURIHandler * handler)
{
  GstIOSAssetSrc *src = GST_IOS_ASSET_SRC (handler);

  return src->uri;
}

static gboolean
gst_ios_asset_src_uri_set_uri (GstURIHandler * handler, const gchar * uri)
{
  GstIOSAssetSrc *src = GST_IOS_ASSET_SRC (handler);
  GError *error = NULL;

  if (! g_str_has_prefix (uri, "assets-library://")) {
    GST_WARNING_OBJECT (src, "Invalid URI '%s' for ios_assetsrc: %s", uri,
        error->message);
    return FALSE;
  }

  return gst_ios_asset_src_set_uri (src, uri);
}

static void
gst_ios_asset_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_ios_asset_src_uri_get_type;
  iface->get_protocols = gst_ios_asset_src_uri_get_protocols;
  iface->get_uri = gst_ios_asset_src_uri_get_uri;
  iface->set_uri = gst_ios_asset_src_uri_set_uri;
}


@implementation GstAssetsLibrary

- (id) init
{
  self = [super init];

  return self;
}

- (void) release
{
}

- (ALAssetRepresentation *) assetForURLSync:(NSURL*) uri
{
  dispatch_semaphore_t sema = dispatch_semaphore_create(0);
  dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT,0);
  ALAssetsLibraryAssetForURLResultBlock resultblock = ^(ALAsset *myasset)
  {
    self->result = [myasset defaultRepresentation];
    dispatch_semaphore_signal(sema);
  };


  ALAssetsLibraryAccessFailureBlock failureblock  = ^(NSError *myerror)
  {
    self->result = nil;
    dispatch_semaphore_signal(sema);
  };

  dispatch_async(queue, ^{
    [self assetForURL:uri resultBlock:resultblock
      failureBlock:failureblock
    ];
  });

  dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
  dispatch_release(sema);

  return self->result;
}
@end
