/* GStreamer
 *
 * unit test for hlsdemux
 *
 * Copyright (C) <2012> Fluendo S.A <support@fluendo.com>
 *  Authors: Andoni Morales Alastruey <amorales@fluendo.com>
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

#include <unistd.h>

#include <gst/check/gstcheck.h>
#include "m3u8.h"
#include "gsthlsadaptation.h"

GST_DEBUG_CATEGORY (fragmented_debug);

static const gchar *INVALID_PLAYLIST = "#EXTM3 UINVALID";

static const gchar *ON_DEMAN_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:10\n\
#EXTINF:10,Test\n\
http://media.example.com/001.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/002.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/003.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/004.ts\n\
#EXT-X-ENDLIST";

static const gchar *DOUBLES_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:10\n\
#EXTINF:10.321,Test\n\
http://media.example.com/001.ts\n\
#EXTINF:9.6789,Test\n\
http://media.example.com/002.ts\n\
#EXTINF:10.2344,Test\n\
http://media.example.com/003.ts\n\
#EXTINF:9.92,Test\n\
http://media.example.com/004.ts\n\
#EXT-X-ENDLIST";

static const gchar *LIVE_PLAYLIST = "#EXTM3U\n\
#EXT-X-TARGETDURATION:8\n\
#EXT-X-MEDIA-SEQUENCE:2680\n\
\n\
#EXTINF:8,\n\
https://priv.example.com/fileSequence2680.ts\n\
#EXTINF:8,\n\
https://priv.example.com/fileSequence2681.ts\n\
#EXTINF:8,\n\
https://priv.example.com/fileSequence2682.ts\n\
#EXTINF:8,\n\
https://priv.example.com/fileSequence2683.ts";

static const gchar *VARIANT_PLAYLIST = "#EXTM3U \n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=128000\n\
http://example.com/low.m3u8\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=256000\n\
http://example.com/mid.m3u8\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=768000\n\
http://example.com/hi.m3u8\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=65000,CODECS=\"mp4a.40.5\"\n\
http://example.com/audio-only.m3u8";

static const gchar *EMPTY_LINES_VARIANT_PLAYLIST = "#EXTM3U \n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=128000\n\n\
http://example.com/low.m3u8\n\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=256000\n\n\
http://example.com/mid.m3u8\n\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=768000\n\n\
http://example.com/hi.m3u8\n\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=65000,CODECS=\"mp4a.40.5\"\n\n\
http://example.com/audio-only.m3u8";

static const gchar *WINDOWS_EMPTY_LINES_VARIANT_PLAYLIST = "#EXTM3U \r\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=128000\r\n\r\n\
http://example.com/low.m3u8\r\n\r\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=256000\r\n\r\n\
http://example.com/mid.m3u8\r\n\r\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=768000\r\n\r\n\
http://example.com/hi.m3u8\r\n\r\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=65000,CODECS=\"mp4a.40.5\"\r\n\r\n\
http://example.com/audio-only.m3u8";

static const gchar *BYTE_RANGES_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:40\n\
#EXTINF:10,Test\n\
#EXT-X-BYTERANGE:1000@100\n\
http://media.example.com/all.ts\n\
#EXTINF:10,Test\n\
#EXT-X-BYTERANGE:1000@1000\n\
http://media.example.com/all.ts\n\
#EXTINF:10,Test\n\
#EXT-X-BYTERANGE:1000@2000\n\
http://media.example.com/all.ts\n\
#EXTINF:10,Test\n\
#EXT-X-BYTERANGE:1000@3000\n\
http://media.example.com/all.ts\n\
#EXT-X-ENDLIST";

static const gchar *BYTE_RANGES_ACC_OFFSET_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:40\n\
#EXTINF:10,Test\n\
#EXT-X-BYTERANGE:1000\n\
http://media.example.com/all.ts\n\
#EXTINF:10,Test\n\
#EXT-X-BYTERANGE:1000\n\
http://media.example.com/all.ts\n\
#EXTINF:10,Test\n\
#EXT-X-BYTERANGE:1000\n\
http://media.example.com/all.ts\n\
#EXTINF:10,Test\n\
#EXT-X-BYTERANGE:1000\n\
http://media.example.com/all.ts\n\
#EXT-X-ENDLIST";

static const gchar *ALTERNATE_AUDIO_PLAYLIST = "#EXTM3U\n\
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"English\",\
  DEFAULT=YES,AUTOSELECT=YES,LANGUAGE=\"en\" \n\
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Deutsche\",\
  DEFAULT=NO,AUTOSELECT=YES,LANGUAGE=\"de\",\
  URI=\"http://localhost/main/german-audio.m3u8\"\n\
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Commentary\",\
  DEFAULT=NO,AUTOSELECT=NO,\
  URI=\"http://localhost/commentary/audio-only.m3u8\"\n\
#EXT-X-STREAM-INF:BANDWIDTH=128000,CODECS=\"avc1.42001f\",AUDIO=\"aac\"\n\
low/video-only.m3u8\n\
#EXT-X-STREAM-INF:BANDWIDTH=256000,CODECS=\"avc1.42001f\",AUDIO=\"aac\"\n\
mid/video-only.m3u8\n\
#EXT-X-STREAM-INF:BANDWIDTH=768000,CODECS=\"avc1.42001f\",AUDIO=\"aac\"\n\
hi/video-only.m3u8\n\
#EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS=\"mp4a.40.5\",AUDIO=\"aac\"\n\
main/english-audio.m3u8";

static const gchar *ALT_AUDIO_PLAYLIST_WITH_VIDEO_AUDIO = "#EXTM3U\n\
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"English\",\
  DEFAULT=YES,AUTOSELECT=YES,LANGUAGE=\"en\" \n\
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Deutsche\",\
  DEFAULT=NO,AUTOSELECT=YES,LANGUAGE=\"de\",\
  URI=\"http://localhost/main/german-audio.m3u8\"\n\
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"aac\",NAME=\"Commentary\",\
  DEFAULT=NO,AUTOSELECT=NO,\
  URI=\"http://localhost/commentary/audio-only.m3u8\"\n\
#EXT-X-STREAM-INF:BANDWIDTH=128000,CODECS=\"avc1.42001f, mp4a.40.5\",AUDIO=\"aac\"\n\
low/video-audio.m3u8\n\
#EXT-X-STREAM-INF:BANDWIDTH=256000,CODECS=\"avc1.42001f, mp4a.40.5\",AUDIO=\"aac\"\n\
mid/video-audio.m3u8\n\
#EXT-X-STREAM-INF:BANDWIDTH=768000,CODECS=\"avc1.42001f, mp4a.40.5\",AUDIO=\"aac\"\n\
hi/video-audio.m3u8\n\
#EXT-X-STREAM-INF:BANDWIDTH=65000,CODECS=\"mp4a.40.5\",AUDIO=\"aac\"\n\
main/english-audio.m3u8";

static const gchar *ON_DEMAN_LOW_VIDEO_ONLY_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:10\n\
#EXTINF:10,Test\n\
http://media.example.com/low/video-only-001.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/low/video-only-002.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/low/video-only-003.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/low/video-only-004.ts\n\
#EXT-X-ENDLIST";

static const gchar *ON_DEMAN_MID_VIDEO_ONLY_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:10\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-001.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-002.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-003.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-004.ts\n\
#EXT-X-ENDLIST";

static const gchar *ON_DEMAN_ENGLISH_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:10\n\
#EXTINF:10,Test\n\
http://media.example.com/audio/english-001.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/audio/english-002.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/audio/english-003.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/audio/english-004.ts\n\
#EXT-X-ENDLIST";

static const gchar *ON_DEMAN_GERMAN_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:10\n\
#EXTINF:10,Test\n\
http://media.example.com/audio/german-001.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/audio/german-002.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/audio/german-003.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/audio/german-004.ts\n\
#EXT-X-ENDLIST";

static const gchar *SUBTITLES_PLAYLIST = "#EXTM3U\n\
#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID=\"subs\",NAME=\"English\",\
  DEFAULT=YES,LANGUAGE=\"en\",\
  URI=\"http://localhost/main/subs-en.m3u8\"\n\
#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID=\"subs\",NAME=\"Deutsche\",\
  DEFAULT=NO,LANGUAGE=\"de\",\
  URI=\"http://localhost/main/subs-de.m3u8\"\n\
#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID=\"subs\",NAME=\"Spanish\",\
  DEFAULT=NO,LANGUAGE=\"es\",\
  URI=\"http://localhost/main/subs-es.m3u8\"\n\
#EXT-X-STREAM-INF:BANDWIDTH=128000,CODECS=\"avc1.42001f, mp4a.40.5\",SUBTITLES=\"subs\"\n\
low/video-audio.m3u8\n\
#EXT-X-STREAM-INF:BANDWIDTH=256000,CODECS=\"avc1.42001f, mp4a.40.5\",SUBTITLES=\"subs\"\n\
mid/video-audio.m3u8\n\
#EXT-X-STREAM-INF:BANDWIDTH=768000,CODECS=\"avc1.42001f, mp4a.40.5\",SUBTITLES=\"subs\"\n\
hi/video-audio.m3u8";

static const gchar *AES_128_ENCRYPTED_PLAYLIST = "#EXTM3U \n\
#EXT-X-TARGETDURATION:10\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-001.ts\n\
#EXT-X-KEY:METHOD=NONE\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-002.ts\n\
#EXT-X-KEY:METHOD=AES-128,URI=\"https://priv.example.com/key.bin\"\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-003.ts\n\
#EXT-X-KEY:METHOD=AES-128,URI=\"https://priv.example.com/key2.bin\",IV=0x1\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-004.ts\n\
#EXTINF:10,Test\n\
http://media.example.com/mid/video-only-005.ts\n\
#EXT-X-ENDLIST";

static const gchar *WINDOWS_LINE_ENDINGS_PLAYLIST = "#EXTM3U \r\n\
#EXT-X-TARGETDURATION:10\r\n\
#EXTINF:10,Test\r\n\
http://media.example.com/001.ts\r\n\
#EXTINF:10,Test\r\n\
http://media.example.com/002.ts\r\n\
#EXTINF:10,Test\r\n\
http://media.example.com/003.ts\r\n\
#EXTINF:10,Test\r\n\
http://media.example.com/004.ts\r\n\
#EXT-X-ENDLIST";

static const gchar *WINDOWS_LINE_ENDINGS_VARIANT_PLAYLIST = "#EXTM3U \r\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=128000\r\n\
http://example.com/low.m3u8\r\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=256000\r\n\
http://example.com/mid.m3u8\r\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=768000\r\n\
http://example.com/hi.m3u8\r\n\
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=65000,CODECS=\"mp4a.40.5\"\r\n\
http://example.com/audio-only.m3u8";

static GstM3U8Client *
load_playlist (const gchar * data)
{
  gboolean ret;
  GstM3U8Client *client;
  GstM3U8Stream *stream;

  client = gst_m3u8_client_new ("http://localhost/test.m3u8");
  ret = gst_m3u8_client_parse_main_playlist (client, g_strdup (data));
  assert_equals_int (ret, TRUE);

  assert_equals_int (client->selected_stream != NULL, TRUE);
  stream = GST_M3U8_STREAM (client->main->streams->data);
  assert_equals_int (stream != NULL, TRUE);

  assert_equals_int (client->selected_stream->first, TRUE);

  return client;
}

GST_START_TEST (test_load_main_playlist_invalid)
{
  gboolean ret;
  GstM3U8Client *client = gst_m3u8_client_new ("http://localhost/test.m3u8");

  ret =
      gst_m3u8_client_parse_main_playlist (client, g_strdup (INVALID_PLAYLIST));
  assert_equals_int (ret, FALSE);

  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_load_main_playlist_rendition)
{
  GstM3U8Client *client;

  client = load_playlist (ON_DEMAN_PLAYLIST);

  assert_equals_int (g_list_length (client->main->streams), 1);
  assert_equals_int (g_hash_table_size (client->main->video_rendition_groups),
      0);
  assert_equals_int (g_hash_table_size (client->main->audio_rendition_groups),
      0);

  assert_equals_int (g_list_length (client->selected_stream->selected_video->
          files), 4);
  assert_equals_int (client->sequence, 0);

  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_load_windows_line_endings_playlist)
{
  GstM3U8Client *client;

  client = load_playlist (WINDOWS_LINE_ENDINGS_PLAYLIST);

  assert_equals_int (g_list_length (client->main->streams), 1);
  assert_equals_int (g_hash_table_size (client->main->video_rendition_groups),
      0);
  assert_equals_int (g_hash_table_size (client->main->audio_rendition_groups),
      0);

  assert_equals_int (g_list_length (client->selected_stream->
          selected_video->files), 4);
  assert_equals_int (client->sequence, 0);

  gst_m3u8_client_free (client);
}

GST_END_TEST;

static void
do_test_load_main_playlist_variant (const gchar * playlist)
{
  GstM3U8Client *client;
  GstM3U8Stream *stream;
  GList *tmp;

  client = load_playlist (playlist);

  assert_equals_int (g_hash_table_size (client->main->video_rendition_groups),
      0);
  assert_equals_int (g_hash_table_size (client->main->audio_rendition_groups),
      0);
  assert_equals_int (g_list_length (client->main->streams), 4);

  /* Audio-Only */
  tmp = g_list_first (client->main->streams);
  stream = GST_M3U8_STREAM (tmp->data);
  assert_equals_int (stream->bandwidth, 65000);
  assert_equals_int (stream->program_id, 1);
  assert_equals_string (GST_M3U8 (stream->selected_video)->uri,
      "http://example.com/audio-only.m3u8");
  assert_equals_int (stream->audio_codec, GST_M3U8_MEDIA_CODEC_HE_AAC);
  assert_equals_int (stream->video_codec, GST_M3U8_MEDIA_CODEC_NONE);

  /* Low */
  tmp = g_list_next (tmp);
  stream = GST_M3U8_STREAM (tmp->data);
  assert_equals_int (stream->bandwidth, 128000);
  assert_equals_int (stream->program_id, 1);
  assert_equals_string (GST_M3U8 (stream->selected_video)->uri,
      "http://example.com/low.m3u8");

  /* Mid */
  tmp = g_list_next (tmp);
  stream = GST_M3U8_STREAM (tmp->data);
  assert_equals_int (stream->bandwidth, 256000);
  assert_equals_int (stream->program_id, 1);
  assert_equals_string (GST_M3U8 (stream->selected_video)->uri,
      "http://example.com/mid.m3u8");

  /* High */
  tmp = g_list_next (tmp);
  stream = GST_M3U8_STREAM (tmp->data);
  assert_equals_int (stream->bandwidth, 768000);
  assert_equals_int (stream->program_id, 1);
  assert_equals_string (GST_M3U8 (stream->selected_video)->uri,
      "http://example.com/hi.m3u8");

  /* Check the first playlist is selected */
  assert_equals_int (client->selected_stream != NULL, TRUE);
  assert_equals_int (client->selected_stream->bandwidth, 128000);

  gst_m3u8_client_free (client);
}

GST_START_TEST (test_load_main_playlist_variant)
{
  do_test_load_main_playlist_variant (VARIANT_PLAYLIST);
}

GST_END_TEST;

GST_START_TEST (test_load_windows_line_endings_variant_playlist)
{
  do_test_load_main_playlist_variant (WINDOWS_LINE_ENDINGS_VARIANT_PLAYLIST);
}

GST_END_TEST;

GST_START_TEST (test_load_main_playlist_with_empty_lines)
{
  do_test_load_main_playlist_variant (EMPTY_LINES_VARIANT_PLAYLIST);
}

GST_END_TEST;

GST_START_TEST (test_load_windows_main_playlist_with_empty_lines)
{
  do_test_load_main_playlist_variant (WINDOWS_EMPTY_LINES_VARIANT_PLAYLIST);
}

GST_END_TEST;

GST_START_TEST (test_on_demand_playlist)
{
  GstM3U8Client *client;
  GstM3U8Playlist *pl;
  GstM3U8MediaFile *file;

  client = load_playlist (ON_DEMAN_PLAYLIST);
  pl = client->selected_stream->selected_video;

  /* Sequence should be 0 as it's an ondemand playlist */
  assert_equals_int (client->sequence, 0);
  /* Check that we are not live */
  assert_equals_int (gst_m3u8_client_is_live (client), FALSE);
  /* Check number of entries */
  assert_equals_int (g_list_length (pl->files), 4);
  /* Check first media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_first (pl->files)->data);
  assert_equals_string (file->uri, "http://media.example.com/001.ts");
  assert_equals_int (file->sequence, 0);
  /* Check last media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_last (pl->files)->data);
  assert_equals_string (file->uri, "http://media.example.com/004.ts");
  assert_equals_int (file->sequence, 3);

  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_live_playlist)
{
  GstM3U8Client *client;
  GstM3U8Playlist *pl;
  GstM3U8MediaFile *file;

  client = load_playlist (LIVE_PLAYLIST);

  assert_equals_int (g_list_length (client->main->streams), 1);
  assert_equals_int (g_hash_table_size (client->main->video_rendition_groups),
      0);
  assert_equals_int (g_hash_table_size (client->main->video_rendition_groups),
      0);

  pl = client->selected_stream->selected_video;
  /* Check that we are live */
  assert_equals_int (gst_m3u8_client_is_live (client), TRUE);
  /* Sequence should last - 3 */
  assert_equals_int (client->sequence, 2680);
  /* Check number of entries */
  assert_equals_int (g_list_length (pl->files), 4);
  /* Check first media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_first (pl->files)->data);
  assert_equals_string (file->uri,
      "https://priv.example.com/fileSequence2680.ts");
  assert_equals_int (file->sequence, 2680);
  /* Check last media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_last (pl->files)->data);
  assert_equals_string (file->uri,
      "https://priv.example.com/fileSequence2683.ts");
  assert_equals_int (file->sequence, 2683);

  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_playlist_with_doubles_duration)
{
  GstM3U8Client *client;
  GstM3U8Playlist *pl;
  GstM3U8MediaFile *file;

  client = load_playlist (DOUBLES_PLAYLIST);

  pl = client->selected_stream->selected_video;
  /* Check first media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 0));
  assert_equals_uint64 (file->duration, 10.321 * GST_SECOND);
  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 1));
  assert_equals_uint64 (file->duration, 9.6789 * GST_SECOND);
  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 2));
  assert_equals_uint64 (file->duration, 10.2344 * GST_SECOND);
  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 3));
  assert_equals_uint64 (file->duration, 9.92 * GST_SECOND);
  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_playlist_with_encription)
{
  GstM3U8Client *client;
  GstM3U8Playlist *pl;
  GstM3U8MediaFile *file;

  client = load_playlist (AES_128_ENCRYPTED_PLAYLIST);

  pl = client->selected_stream->selected_video;
  assert_equals_int (g_list_length (pl->files), 5);

  /* Check all media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 0));
  assert_equals_int (file->enc_method, GST_FRAGMENT_ENCODING_METHOD_NONE);

  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 1));
  assert_equals_int (file->enc_method, GST_FRAGMENT_ENCODING_METHOD_NONE);

  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 2));
  assert_equals_int (file->enc_method, GST_FRAGMENT_ENCODING_METHOD_AES_128);
  assert_equals_string (file->key_url, "https://priv.example.com/key.bin");
  assert_equals_string (file->iv, "0x00000000000000000000000000000002");

  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 3));
  assert_equals_int (file->enc_method, GST_FRAGMENT_ENCODING_METHOD_AES_128);
  assert_equals_string (file->key_url, "https://priv.example.com/key2.bin");
  assert_equals_string (file->iv, "0x1");

  file = GST_M3U8_MEDIA_FILE (g_list_nth_data (pl->files, 4));
  assert_equals_int (file->enc_method, GST_FRAGMENT_ENCODING_METHOD_AES_128);
  assert_equals_string (file->key_url, "https://priv.example.com/key2.bin");
  assert_equals_string (file->iv, "0x1");

  gst_m3u8_client_free (client);
}

GST_END_TEST;
GST_START_TEST (test_update_invalid_playlist)
{
  GstM3U8Client *client;
  GstM3U8Playlist *pl;
  gboolean ret;

  /* Test updates in on-demand playlists */
  client = load_playlist (ON_DEMAN_PLAYLIST);
  pl = client->selected_stream->selected_video;
  assert_equals_int (g_list_length (pl->files), 4);
  ret = gst_m3u8_client_update (client, g_strdup ("#INVALID"), NULL, NULL);
  assert_equals_int (ret, FALSE);

  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_update_playlist)
{
  GstM3U8Client *client;
  GstM3U8Playlist *pl;
  gchar *live_pl;
  gboolean ret;

  /* Test updates in on-demand playlists */
  client = load_playlist (ON_DEMAN_PLAYLIST);
  pl = client->selected_stream->selected_video;
  assert_equals_int (g_list_length (pl->files), 4);
  ret =
      gst_m3u8_client_update (client, g_strdup (ON_DEMAN_PLAYLIST), NULL, NULL);
  assert_equals_int (ret, TRUE);
  assert_equals_int (g_list_length (pl->files), 4);
  gst_m3u8_client_free (client);

  /* Test updates in live playlists */
  client = load_playlist (LIVE_PLAYLIST);
  pl = client->selected_stream->selected_video;
  assert_equals_int (g_list_length (pl->files), 4);
  /* Add a new entry to the playlist and check the update */
  live_pl = g_strdup_printf ("%s\n%s\n%s", LIVE_PLAYLIST, "#EXTINF:8",
      "https://priv.example.com/fileSequence2683.ts");
  ret = gst_m3u8_client_update (client, live_pl, NULL, NULL);
  assert_equals_int (ret, TRUE);
  assert_equals_int (g_list_length (pl->files), 5);
  /* Test sliding window */
  ret = gst_m3u8_client_update (client, g_strdup (LIVE_PLAYLIST), NULL, NULL);
  assert_equals_int (ret, TRUE);
  assert_equals_int (g_list_length (pl->files), 4);
  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_playlist_media_files)
{
  GstM3U8Client *client;
  GstM3U8Playlist *pl;
  GstM3U8MediaFile *file;

  client = load_playlist (ON_DEMAN_PLAYLIST);
  pl = client->selected_stream->selected_video;

  /* Check number of entries */
  assert_equals_int (g_list_length (pl->files), 4);
  /* Check first media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_first (pl->files)->data);
  assert_equals_string (file->uri, "http://media.example.com/001.ts");
  assert_equals_int (file->sequence, 0);
  assert_equals_float (file->duration, 10 * (double) GST_SECOND);
  assert_equals_int (file->offset, -1);
  assert_equals_int (file->length, -1);
  /* FIXME */
  /*assert_equals_string (file->title, "Test"); */

  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_playlist_byte_range_media_files)
{
  GstM3U8Client *client;
  GstM3U8Playlist *pl;
  GstM3U8MediaFile *file;

  client = load_playlist (BYTE_RANGES_PLAYLIST);
  pl = client->selected_stream->selected_video;

  /* Check number of entries */
  assert_equals_int (g_list_length (pl->files), 4);
  /* Check first media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_first (pl->files)->data);
  assert_equals_string (file->uri, "http://media.example.com/all.ts");
  assert_equals_int (file->sequence, 0);
  assert_equals_float (file->duration, 10 * (double) GST_SECOND);
  assert_equals_int (file->offset, 100);
  assert_equals_int (file->length, 1000);
  /* Check last media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_last (pl->files)->data);
  assert_equals_string (file->uri, "http://media.example.com/all.ts");
  assert_equals_int (file->sequence, 3);
  assert_equals_float (file->duration, 10 * (double) GST_SECOND);
  assert_equals_int (file->offset, 3000);
  assert_equals_int (file->length, 1000);

  gst_m3u8_client_free (client);


  client = load_playlist (BYTE_RANGES_ACC_OFFSET_PLAYLIST);
  pl = client->selected_stream->selected_video;

  /* Check number of entries */
  assert_equals_int (g_list_length (pl->files), 4);
  /* Check first media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_first (pl->files)->data);
  assert_equals_string (file->uri, "http://media.example.com/all.ts");
  assert_equals_int (file->sequence, 0);
  assert_equals_float (file->duration, 10 * (double) GST_SECOND);
  assert_equals_int (file->offset, 0);
  assert_equals_int (file->length, 1000);
  /* Check last media segments */
  file = GST_M3U8_MEDIA_FILE (g_list_last (pl->files)->data);
  assert_equals_string (file->uri, "http://media.example.com/all.ts");
  assert_equals_int (file->sequence, 3);
  assert_equals_float (file->duration, 10 * (double) GST_SECOND);
  assert_equals_int (file->offset, 3000);
  assert_equals_int (file->length, 1000);

  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_get_next_fragment)
{
  GstM3U8Client *client;
  GstFragment *v_frag = NULL, *a_frag = NULL, *s_frag = NULL;

  client = load_playlist (BYTE_RANGES_PLAYLIST);

  /* Check the next fragment */
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (s_frag == NULL, TRUE);
  assert_equals_int (a_frag == NULL, TRUE);
  assert_equals_int (v_frag != NULL, TRUE);
  assert_equals_int (v_frag->discontinuous, FALSE);
  assert_equals_string (v_frag->name, "http://media.example.com/all.ts");
  assert_equals_uint64 (v_frag->start_time, 0);
  assert_equals_uint64 (v_frag->stop_time, 10 * GST_SECOND);
  assert_equals_uint64 (v_frag->offset, 100);
  assert_equals_uint64 (v_frag->length, 1000);
  g_object_unref (v_frag);

  /* Check next media segments */
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (s_frag == NULL, TRUE);
  assert_equals_int (v_frag != NULL, TRUE);
  assert_equals_int (v_frag->discontinuous, FALSE);
  assert_equals_string (v_frag->name, "http://media.example.com/all.ts");
  assert_equals_uint64 (v_frag->start_time, 10 * GST_SECOND);
  assert_equals_uint64 (v_frag->stop_time, 20 * GST_SECOND);
  assert_equals_uint64 (v_frag->offset, 1000);
  assert_equals_uint64 (v_frag->length, 1000);
  g_object_unref (v_frag);

  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_get_current_position)
{
  GstM3U8Client *client;
  GstClockTime pos;
  GstFragment *v_frag = NULL, *a_frag = NULL, *s_frag = NULL;

  client = load_playlist (BYTE_RANGES_PLAYLIST);

  /* Check the next fragment */
  gst_m3u8_client_get_current_position (client, &pos, NULL);
  assert_equals_uint64 (pos, 0);

  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  gst_m3u8_client_get_current_position (client, &pos, NULL);
  assert_equals_uint64 (pos, 10 * GST_SECOND);

  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  gst_m3u8_client_get_current_position (client, &pos, NULL);
  assert_equals_uint64 (pos, 20 * GST_SECOND);

  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_get_duration)
{
  GstM3U8Client *client;

  /* Test duration for on-demand playlists */
  client = load_playlist (ON_DEMAN_PLAYLIST);
  assert_equals_uint64 (gst_m3u8_client_get_duration (client), 40 * GST_SECOND);
  gst_m3u8_client_free (client);

  /* Test duration for live playlists */
  client = load_playlist (LIVE_PLAYLIST);
  assert_equals_uint64 (gst_m3u8_client_get_duration (client),
      GST_CLOCK_TIME_NONE);
  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_get_target_duration)
{
  GstM3U8Client *client;

  client = load_playlist (ON_DEMAN_PLAYLIST);
  assert_equals_uint64 (gst_m3u8_client_get_target_duration (client),
      10 * GST_SECOND);

  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_get_streams_bitrates)
{
  GstM3U8Client *client;
  GList *bandwidths;

  client = load_playlist (VARIANT_PLAYLIST);
  bandwidths = gst_m3u8_client_get_streams_bitrates (client);
  assert_equals_int (g_list_length (bandwidths), 4);
  assert_equals_int (GPOINTER_TO_UINT (g_list_nth_data (bandwidths, 0)), 65000);
  assert_equals_int (GPOINTER_TO_UINT (g_list_nth_data (bandwidths, 3)),
      768000);

  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_get_stream_for_bitrate)
{
  GstM3U8Client *client;
  GstM3U8Stream *stream;

  client = load_playlist (VARIANT_PLAYLIST);
  stream = gst_m3u8_client_get_stream_for_bitrate (client, 0);
  assert_equals_int (stream->bandwidth, 65000);
  stream = gst_m3u8_client_get_stream_for_bitrate (client, G_MAXINT32);
  assert_equals_int (stream->bandwidth, 768000);
  stream = gst_m3u8_client_get_stream_for_bitrate (client, 300000);
  assert_equals_int (stream->bandwidth, 256000);
  stream = gst_m3u8_client_get_stream_for_bitrate (client, 500000);
  assert_equals_int (stream->bandwidth, 256000);
  stream = gst_m3u8_client_get_stream_for_bitrate (client, 255000);
  assert_equals_int (stream->bandwidth, 128000);


  gst_m3u8_client_free (client);
}

GST_END_TEST;

static void
do_test_seek (GstM3U8Client * client, guint seek_pos, gint pos)
{
  GstClockTime cur_pos;
  gboolean ret;

  ret = gst_m3u8_client_seek (client, seek_pos * GST_SECOND);
  if (pos == -1) {
    assert_equals_int (ret, FALSE);
    return;
  }
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_current_position (client, &cur_pos, NULL);
  assert_equals_uint64 (cur_pos, pos * GST_SECOND);
}

GST_START_TEST (test_seek)
{
  GstM3U8Client *client;

  client = load_playlist (ON_DEMAN_PLAYLIST);

  /* Test seek in the middle of a fragment */
  do_test_seek (client, 1, 0);
  do_test_seek (client, 11, 10);
  do_test_seek (client, 22, 20);
  do_test_seek (client, 39, 30);

  /* Test exact seeks */
  do_test_seek (client, 0, 0);
  do_test_seek (client, 10, 10);
  do_test_seek (client, 20, 20);
  do_test_seek (client, 30, 30);

  /* Test invalid seeks (end if list should be 30 + 10) */
  do_test_seek (client, 39, 30);
  do_test_seek (client, 40, -1);
  gst_m3u8_client_free (client);

  /* Test seeks on a live playlist */
  client = load_playlist (LIVE_PLAYLIST);
  do_test_seek (client, 0, 0);

  do_test_seek (client, 8, 8);
  do_test_seek (client, 20, 16);
  do_test_seek (client, 30, 24);

  do_test_seek (client, 3000, -1);
  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_alternate_audio_playlist)
{
  GstM3U8Client *client;
  GstM3U8Media *media;
  GList *alternates;

  client = load_playlist (ALTERNATE_AUDIO_PLAYLIST);

  assert_equals_int (g_list_length (client->main->streams), 4);
  assert_equals_int (g_hash_table_size (client->main->video_rendition_groups),
      0);
  assert_equals_int (g_hash_table_size (client->main->audio_rendition_groups),
      1);
  assert_equals_int (g_hash_table_size (client->
          selected_stream->audio_alternates), 3);
  assert_equals_int (g_hash_table_size (client->
          selected_stream->video_alternates), 0);

  alternates =
      g_hash_table_lookup (client->main->audio_rendition_groups, "aac");
  assert_equals_int (alternates != NULL, TRUE);
  media = GST_M3U8_MEDIA (g_list_nth_data (alternates, 0));
  assert_equals_int (media->media_type, GST_M3U8_MEDIA_TYPE_AUDIO);
  assert_equals_string (media->group_id, "aac");
  assert_equals_string (media->name, "English");
  assert_equals_string (media->language, "en");
  assert_equals_string (media->uri, "http://localhost/main/english-audio.m3u8");
  assert_equals_string (media->uri, GST_M3U8 (media->playlist)->uri);
  assert_equals_int (media->is_default, TRUE);
  assert_equals_int (media->autoselect, TRUE);

  assert_equals_int (g_hash_table_size (client->
          selected_stream->audio_alternates), 3);
  /* Check the list of audio alternates */
  alternates = gst_m3u8_client_get_alternates (client,
      GST_M3U8_MEDIA_TYPE_AUDIO);
  assert_equals_int (g_list_length (alternates), 3);
  /* Default comes always first */
  assert_equals_string (g_list_nth_data (alternates, 0), "English");
  assert_equals_string (g_list_nth_data (alternates, 1), "Commentary");
  assert_equals_string (g_list_nth_data (alternates, 2), "Deutsche");

  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_subtitles_playlist)
{
  GstM3U8Client *client;
  GstM3U8Media *media;
  GList *alternates;

  client = load_playlist (SUBTITLES_PLAYLIST);

  assert_equals_int (g_list_length (client->main->streams), 3);
  assert_equals_int (g_hash_table_size (client->main->video_rendition_groups),
      0);
  assert_equals_int (g_hash_table_size (client->main->audio_rendition_groups),
      0);
  assert_equals_int (g_hash_table_size (client->main->subtt_rendition_groups),
      1);
  assert_equals_int (g_hash_table_size (client->
          selected_stream->audio_alternates), 0);
  assert_equals_int (g_hash_table_size (client->
          selected_stream->video_alternates), 0);
  assert_equals_int (g_hash_table_size (client->
          selected_stream->subtt_alternates), 3);

  alternates =
      g_hash_table_lookup (client->main->subtt_rendition_groups, "subs");
  assert_equals_int (alternates != NULL, TRUE);
  media = GST_M3U8_MEDIA (g_list_nth_data (alternates, 0));
  assert_equals_int (media->media_type, GST_M3U8_MEDIA_TYPE_SUBTITLES);
  assert_equals_string (media->group_id, "subs");
  assert_equals_string (media->name, "English");
  assert_equals_string (media->language, "en");
  assert_equals_string (media->uri, "http://localhost/main/subs-en.m3u8");
  assert_equals_string (media->uri, GST_M3U8 (media->playlist)->uri);
  assert_equals_int (media->is_default, TRUE);
  assert_equals_int (media->autoselect, FALSE);

  /* Check the list of subtitles */
  alternates = gst_m3u8_client_get_alternates (client,
      GST_M3U8_MEDIA_TYPE_SUBTITLES);
  assert_equals_int (g_list_length (alternates), 3);
  assert_equals_string (g_list_nth_data (alternates, 0), "Deutsche");
  assert_equals_string (g_list_nth_data (alternates, 1), "Spanish");
  assert_equals_string (g_list_nth_data (alternates, 2), "English");

  gst_m3u8_client_free (client);
}

GST_END_TEST;
GST_START_TEST (test_select_subs_alternate)
{
  GstM3U8Client *client;
  const gchar *a_uri, *v_uri, *s_uri;
  gboolean ret;

  /* Check with a playlist with alternative audio renditions where the video
   * stream is video-only and therefor we always have 2 playlists, one for
   * video and another one for audio */
  client = load_playlist (SUBTITLES_PLAYLIST);
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri == NULL, TRUE);
  assert_equals_int (s_uri != NULL, TRUE);
  assert_equals_string (s_uri, "http://localhost/main/subs-de.m3u8");
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-audio.m3u8");

  ret =
      gst_m3u8_client_set_alternate (client, GST_M3U8_MEDIA_TYPE_SUBTITLES,
      "English");
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri == NULL, TRUE);
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-audio.m3u8");
  assert_equals_int (s_uri != NULL, TRUE);
  assert_equals_string (s_uri, "http://localhost/main/subs-en.m3u8");

  ret =
      gst_m3u8_client_set_alternate (client, GST_M3U8_MEDIA_TYPE_SUBTITLES,
      "Spanish");
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri == NULL, TRUE);
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-audio.m3u8");
  assert_equals_int (s_uri != NULL, TRUE);
  assert_equals_string (s_uri, "http://localhost/main/subs-es.m3u8");

  ret =
      gst_m3u8_client_set_alternate (client, GST_M3U8_MEDIA_TYPE_SUBTITLES,
      NULL);
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri == NULL, TRUE);
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-audio.m3u8");
  assert_equals_int (s_uri == NULL, TRUE);

  gst_m3u8_client_free (client);
}

GST_END_TEST;
GST_START_TEST (test_select_alternate)
{
  GstM3U8Client *client;
  const gchar *a_uri, *v_uri, *s_uri;
  gboolean ret;

  /* Check with a playlist with alternative audio renditions where the video
   * stream is video-only and therefor we always have 2 playlists, one for
   * video and another one for audio */
  client = load_playlist (ALTERNATE_AUDIO_PLAYLIST);
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri != NULL, TRUE);
  assert_equals_string (a_uri, "http://localhost/main/english-audio.m3u8");
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-only.m3u8");
  assert_equals_int (s_uri == NULL, TRUE);

  ret =
      gst_m3u8_client_set_alternate (client, GST_M3U8_MEDIA_TYPE_AUDIO,
      "Deutsche");
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri != NULL, TRUE);
  assert_equals_string (a_uri, "http://localhost/main/german-audio.m3u8");
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-only.m3u8");
  assert_equals_int (s_uri == NULL, TRUE);

  /* Check that selecting the audio-only fallback stream we only have the audio
   * uri */
  gst_m3u8_client_set_current (client,
      GST_M3U8_STREAM (client->main->streams->data));
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri != NULL, TRUE);
  assert_equals_string (a_uri, "http://localhost/main/german-audio.m3u8");
  assert_equals_int (v_uri == NULL, TRUE);
  assert_equals_int (s_uri == NULL, TRUE);

  gst_m3u8_client_free (client);

  /* Now check with a playlist with alternative audio renditions where the
   * video * stream has the default audio rendition muxed and therefore we
   * only have 2 playlists when the audio alternative rendition is not the
   * default one */
  client = load_playlist (ALT_AUDIO_PLAYLIST_WITH_VIDEO_AUDIO);
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri == NULL, TRUE);
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-audio.m3u8");
  assert_equals_int (s_uri == NULL, TRUE);

  /* Check that selecting the audio-only fallback stream we only have the audio
   * uri */
  gst_m3u8_client_set_current (client,
      GST_M3U8_STREAM (client->main->streams->data));
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri != NULL, TRUE);
  assert_equals_string (a_uri, "http://localhost/main/english-audio.m3u8");
  assert_equals_int (v_uri == NULL, TRUE);
  assert_equals_int (s_uri == NULL, TRUE);

  /* Get back to the audio-video stream */
  gst_m3u8_client_set_current (client,
      GST_M3U8_STREAM (client->main->streams->next->data));
  /* Now set a different audio and check that we have 2 playlists */
  ret =
      gst_m3u8_client_set_alternate (client, GST_M3U8_MEDIA_TYPE_AUDIO,
      "Deutsche");
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri != NULL, TRUE);
  assert_equals_string (a_uri, "http://localhost/main/german-audio.m3u8");
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-audio.m3u8");
  assert_equals_int (s_uri == NULL, TRUE);

  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_simulation)
{
  GstM3U8Client *client;
  const gchar *a_uri, *v_uri, *s_uri;
  GstFragment *a_frag, *v_frag, *s_frag;
  gboolean ret;

  client = load_playlist (ALTERNATE_AUDIO_PLAYLIST);
  /* The default selection should be audio-only, which only has audio and not
   * video */
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri != NULL, TRUE);
  assert_equals_string (a_uri, "http://localhost/main/english-audio.m3u8");
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/low/video-only.m3u8");
  assert_equals_int (s_uri == NULL, TRUE);

  /* Update the playlists */
  ret = gst_m3u8_client_update (client,
      g_strdup (ON_DEMAN_LOW_VIDEO_ONLY_PLAYLIST),
      g_strdup (ON_DEMAN_ENGLISH_PLAYLIST), NULL);
  assert_equals_int (ret, TRUE);
  assert_equals_int (g_list_length (client->selected_stream->selected_video->
          files), 4);
  assert_equals_int (g_list_length (client->selected_stream->selected_audio->
          files), 4);

  /* Get the first fragment */
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (v_frag != NULL, TRUE);
  assert_equals_int (a_frag != NULL, TRUE);
  assert_equals_string (v_frag->name,
      "http://media.example.com/low/video-only-001.ts");
  assert_equals_string (a_frag->name,
      "http://media.example.com/audio/english-001.ts");
  g_object_unref (v_frag);
  g_object_unref (a_frag);

  /* Get the next fragment */
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (v_frag != NULL, TRUE);
  assert_equals_int (a_frag != NULL, TRUE);
  assert_equals_string (v_frag->name,
      "http://media.example.com/low/video-only-002.ts");
  assert_equals_string (a_frag->name,
      "http://media.example.com/audio/english-002.ts");
  g_object_unref (v_frag);
  g_object_unref (a_frag);

  /* Switch to German audio */
  ret =
      gst_m3u8_client_set_alternate (client, GST_M3U8_MEDIA_TYPE_AUDIO,
      "Deutsche");
  assert_equals_int (ret, TRUE);
  /* Get the new uri's */
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri != NULL, TRUE);
  assert_equals_string (a_uri, "http://localhost/main/german-audio.m3u8");
  /* On demand  so the uri does not need to be downloaded again */
  assert_equals_int (v_uri == NULL, TRUE);
  assert_equals_int (s_uri == NULL, TRUE);
  /* Update the new uri's */
  ret =
      gst_m3u8_client_update (client,
      g_strdup (ON_DEMAN_LOW_VIDEO_ONLY_PLAYLIST),
      g_strdup (ON_DEMAN_GERMAN_PLAYLIST), NULL);
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (s_frag == NULL, TRUE);
  assert_equals_int (v_frag != NULL, TRUE);
  assert_equals_int (a_frag != NULL, TRUE);
  assert_equals_string (a_frag->name,
      "http://media.example.com/audio/german-003.ts");
  assert_equals_string (v_frag->name,
      "http://media.example.com/low/video-only-003.ts");
  g_object_unref (v_frag);
  g_object_unref (a_frag);

  /* Switch to a higher bitrate */
  gst_m3u8_client_set_current (client,
      gst_m3u8_client_get_stream_for_bitrate (client, 260000));
  gst_m3u8_client_get_current_uri (client, &v_uri, &a_uri, &s_uri);
  assert_equals_int (a_uri != NULL, TRUE);
  assert_equals_string (a_uri, "http://localhost/main/german-audio.m3u8");
  assert_equals_int (v_uri != NULL, TRUE);
  assert_equals_string (v_uri, "http://localhost/mid/video-only.m3u8");
  assert_equals_int (s_uri == NULL, TRUE);
  ret =
      gst_m3u8_client_update (client,
      g_strdup (ON_DEMAN_MID_VIDEO_ONLY_PLAYLIST),
      g_strdup (ON_DEMAN_GERMAN_PLAYLIST), NULL);
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (s_frag == NULL, TRUE);
  assert_equals_int (a_frag != NULL, TRUE);
  assert_equals_int (v_frag != NULL, TRUE);
  assert_equals_string (a_frag->name,
      "http://media.example.com/audio/german-004.ts");
  assert_equals_string (v_frag->name,
      "http://media.example.com/mid/video-only-004.ts");
  g_object_unref (v_frag);
  g_object_unref (a_frag);

  /* Seek to the beginning */
  gst_m3u8_client_seek (client, 0);
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (s_frag == NULL, TRUE);
  assert_equals_int (a_frag != NULL, TRUE);
  assert_equals_int (v_frag != NULL, TRUE);
  assert_equals_string (a_frag->name,
      "http://media.example.com/audio/german-001.ts");
  assert_equals_string (v_frag->name,
      "http://media.example.com/mid/video-only-001.ts");
  g_object_unref (v_frag);
  g_object_unref (a_frag);

  /* Select English audio again */
  ret =
      gst_m3u8_client_set_alternate (client, GST_M3U8_MEDIA_TYPE_AUDIO,
      "English");
  assert_equals_int (ret, TRUE);
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (s_frag == NULL, TRUE);
  assert_equals_int (a_frag != NULL, TRUE);
  assert_equals_int (v_frag != NULL, TRUE);
  assert_equals_string (a_frag->name,
      "http://media.example.com/audio/english-002.ts");
  assert_equals_string (v_frag->name,
      "http://media.example.com/mid/video-only-002.ts");
  g_object_unref (v_frag);
  g_object_unref (a_frag);

  /* Go to the audio-only fallback */
  gst_m3u8_client_set_current (client,
      gst_m3u8_client_get_stream_for_bitrate (client, 20000));
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (s_frag == NULL, TRUE);
  assert_equals_int (a_frag != NULL, TRUE);
  assert_equals_int (v_frag == NULL, TRUE);
  assert_equals_string (a_frag->name,
      "http://media.example.com/audio/english-003.ts");
  g_object_unref (a_frag);

  /* Go to mid again */
  gst_m3u8_client_set_current (client,
      gst_m3u8_client_get_stream_for_bitrate (client, 260000));
  gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (s_frag == NULL, TRUE);
  assert_equals_int (a_frag != NULL, TRUE);
  assert_equals_int (v_frag != NULL, TRUE);
  assert_equals_string (a_frag->name,
      "http://media.example.com/audio/english-004.ts");
  assert_equals_string (v_frag->name,
      "http://media.example.com/mid/video-only-004.ts");
  g_object_unref (a_frag);
  g_object_unref (v_frag);

  /* End of stream */
  ret = gst_m3u8_client_get_next_fragment (client, &v_frag, &a_frag, &s_frag);
  assert_equals_int (ret, FALSE);

  gst_m3u8_client_free (client);
}

GST_END_TEST;

GST_START_TEST (test_adaptation_add_fragments)
{
  GstHLSAdaptation *adaptation;
  GstHLSAdaptationFragment *fragment;

  adaptation = gst_hls_adaptation_new ();
  adaptation->max_fragments = 3;
  assert_equals_int (g_list_length (adaptation->fragments), 0);

  gst_hls_adaptation_add_fragment (adaptation, 100000, 1 * GST_SECOND);
  assert_equals_int (g_list_length (adaptation->fragments), 1);
  fragment =
      (GstHLSAdaptationFragment *) g_list_last (adaptation->fragments)->data;
  assert_equals_int (100000, fragment->size);
  assert_equals_uint64 (fragment->download_time, 1 * GST_SECOND);

  gst_hls_adaptation_add_fragment (adaptation, 200000, 2 * GST_SECOND);
  assert_equals_int (g_list_length (adaptation->fragments), 2);
  fragment =
      (GstHLSAdaptationFragment *) g_list_last (adaptation->fragments)->data;
  assert_equals_int (fragment->size, 200000);
  assert_equals_uint64 (fragment->download_time, 2 * GST_SECOND);

  gst_hls_adaptation_add_fragment (adaptation, 300000, 3 * GST_SECOND);
  assert_equals_int (g_list_length (adaptation->fragments), 3);
  fragment =
      (GstHLSAdaptationFragment *) g_list_last (adaptation->fragments)->data;
  assert_equals_int (fragment->size, 300000);
  assert_equals_uint64 (fragment->download_time, 3 * GST_SECOND);

  gst_hls_adaptation_add_fragment (adaptation, 400000, 4 * GST_SECOND);
  assert_equals_int (g_list_length (adaptation->fragments), 3);
  fragment =
      (GstHLSAdaptationFragment *) g_list_last (adaptation->fragments)->data;
  assert_equals_int (fragment->size, 400000);
  assert_equals_uint64 (fragment->download_time, 4 * GST_SECOND);
  fragment =
      (GstHLSAdaptationFragment *) g_list_first (adaptation->fragments)->data;
  assert_equals_int (fragment->size, 200000);
  assert_equals_uint64 (fragment->download_time, 2 * GST_SECOND);

  gst_hls_adaptation_free (adaptation);
}

GST_END_TEST;

GST_START_TEST (test_adaptation_reset)
{
  GstHLSAdaptation *adaptation;

  adaptation = gst_hls_adaptation_new ();
  adaptation->max_fragments = 3;
  assert_equals_int (g_list_length (adaptation->fragments), 0);

  gst_hls_adaptation_add_fragment (adaptation, 100000, 1 * GST_SECOND);
  assert_equals_int (g_list_length (adaptation->fragments), 1);
  gst_hls_adaptation_add_fragment (adaptation, 200000, 2 * GST_SECOND);
  assert_equals_int (g_list_length (adaptation->fragments), 2);
  gst_hls_adaptation_add_fragment (adaptation, 300000, 3 * GST_SECOND);
  assert_equals_int (g_list_length (adaptation->fragments), 3);
  gst_hls_adaptation_reset (adaptation);
  assert_equals_int (g_list_length (adaptation->fragments), 0);

  gst_hls_adaptation_free (adaptation);
}

GST_END_TEST;

GST_START_TEST (test_adaptation_always_lowest)
{
  GstHLSAdaptation *adaptation;
  guint bitrate;

  adaptation = gst_hls_adaptation_new ();
  gst_hls_adaptation_add_stream (adaptation, 50000);
  gst_hls_adaptation_add_stream (adaptation, 100000);
  gst_hls_adaptation_add_stream (adaptation, 400000);
  gst_hls_adaptation_add_stream (adaptation, 1000000);
  gst_hls_adaptation_set_algorithm_func (adaptation,
      gst_hls_adaptation_always_lowest);

  bitrate = gst_hls_adaptation_get_target_bitrate (adaptation);
  assert_equals_int (bitrate, 50000);

  gst_hls_adaptation_set_max_bitrate (adaptation, 500000);
  bitrate = gst_hls_adaptation_get_target_bitrate (adaptation);
  assert_equals_int (bitrate, 50000);

  gst_hls_adaptation_set_connection_speed (adaptation, 500000);
  bitrate = gst_hls_adaptation_get_target_bitrate (adaptation);
  assert_equals_int (bitrate, 50000);

  gst_hls_adaptation_free (adaptation);
}

GST_END_TEST;

GST_START_TEST (test_adaptation_always_highest)
{
  GstHLSAdaptation *adaptation;
  guint bitrate;

  adaptation = gst_hls_adaptation_new ();
  gst_hls_adaptation_add_stream (adaptation, 50000);
  gst_hls_adaptation_add_stream (adaptation, 100000);
  gst_hls_adaptation_add_stream (adaptation, 400000);
  gst_hls_adaptation_add_stream (adaptation, 1000000);
  gst_hls_adaptation_set_algorithm_func (adaptation,
      gst_hls_adaptation_always_highest);

  bitrate = gst_hls_adaptation_get_target_bitrate (adaptation);
  assert_equals_int (bitrate, 1000000);

  gst_hls_adaptation_set_connection_speed (adaptation, 500000);
  bitrate = gst_hls_adaptation_get_target_bitrate (adaptation);
  assert_equals_int (bitrate, 400000);

  gst_hls_adaptation_set_max_bitrate (adaptation, 1.2);
  bitrate = gst_hls_adaptation_get_target_bitrate (adaptation);
  assert_equals_int (bitrate, 400000);


  gst_hls_adaptation_free (adaptation);
}

GST_END_TEST;

GST_START_TEST (test_adaptation_fixed_bitrate)
{
  GstHLSAdaptation *adaptation;
  guint bitrate;

  adaptation = gst_hls_adaptation_new ();
  gst_hls_adaptation_add_stream (adaptation, 50000);
  gst_hls_adaptation_add_stream (adaptation, 100000);
  gst_hls_adaptation_add_stream (adaptation, 400000);
  gst_hls_adaptation_add_stream (adaptation, 1000000);
  gst_hls_adaptation_set_algorithm_func (adaptation,
      gst_hls_adaptation_fixed_bitrate);

  bitrate = gst_hls_adaptation_get_target_bitrate (adaptation);
  assert_equals_int (bitrate, 0);

  gst_hls_adaptation_set_connection_speed (adaptation, 400000);
  bitrate = gst_hls_adaptation_get_target_bitrate (adaptation);
  assert_equals_int (bitrate, 400000);

  gst_hls_adaptation_set_connection_speed (adaptation, 620000);
  bitrate = gst_hls_adaptation_get_target_bitrate (adaptation);
  assert_equals_int (bitrate, 620000);
  gst_hls_adaptation_free (adaptation);
}

GST_END_TEST;
GST_START_TEST (test_adaptation_bandwidth_estimation)
{
  GstHLSAdaptation *adaptation;
  guint bitrate;

  adaptation = gst_hls_adaptation_new ();
  gst_hls_adaptation_add_stream (adaptation, 50000);
  gst_hls_adaptation_add_stream (adaptation, 100000);
  gst_hls_adaptation_add_stream (adaptation, 400000);
  gst_hls_adaptation_add_stream (adaptation, 1000000);
  gst_hls_adaptation_set_algorithm_func (adaptation,
      gst_hls_adaptation_bandwidth_estimation);

  bitrate = gst_hls_adaptation_get_target_bitrate (adaptation);
  assert_equals_int (bitrate, -1);

  gst_hls_adaptation_add_fragment (adaptation, 100000, 1 * GST_SECOND);
  bitrate = gst_hls_adaptation_get_target_bitrate (adaptation);
  assert_equals_int (bitrate, 800000);

  /* The download is slower now and the average bitrate decreases */
  gst_hls_adaptation_add_fragment (adaptation, 100000, 1.3 * GST_SECOND);
  bitrate = gst_hls_adaptation_get_target_bitrate (adaptation);
  assert_equals_int (bitrate, 665035);

  /* The average bitrate should increase now slowly as the slow
   * download has less and less weigth*/
  gst_hls_adaptation_add_fragment (adaptation, 100000, 1 * GST_SECOND);
  bitrate = gst_hls_adaptation_get_target_bitrate (adaptation);
  assert_equals_int (bitrate, 754819);

  gst_hls_adaptation_add_fragment (adaptation, 100000, 1 * GST_SECOND);
  bitrate = gst_hls_adaptation_get_target_bitrate (adaptation);
  assert_equals_int (bitrate, 783911);

  gst_hls_adaptation_add_fragment (adaptation, 100000, 1 * GST_SECOND);
  bitrate = gst_hls_adaptation_get_target_bitrate (adaptation);
  assert_equals_int (bitrate, 794150);

  gst_hls_adaptation_add_fragment (adaptation, 100000, 1 * GST_SECOND);
  bitrate = gst_hls_adaptation_get_target_bitrate (adaptation);
  assert_equals_int (bitrate, 797848);

  /* We are back to original bitrate average now that the slow download is
   * gone */
  gst_hls_adaptation_add_fragment (adaptation, 100000, 1 * GST_SECOND);
  bitrate = gst_hls_adaptation_get_target_bitrate (adaptation);
  assert_equals_int (bitrate, 800000);

  gst_hls_adaptation_free (adaptation);
}

GST_END_TEST;

static Suite *
hlsdemux_suite (void)
{
  Suite *s = suite_create ("hlsdemux");
  TCase *tc_m3u8 = tcase_create ("m3u8client");
  TCase *tc_adaptation = tcase_create ("adaptation");

  suite_add_tcase (s, tc_m3u8);
  tcase_add_test (tc_m3u8, test_load_main_playlist_invalid);
  tcase_add_test (tc_m3u8, test_load_main_playlist_rendition);
  tcase_add_test (tc_m3u8, test_load_windows_line_endings_playlist);
  tcase_add_test (tc_m3u8, test_load_main_playlist_variant);
  tcase_add_test (tc_m3u8, test_load_windows_line_endings_variant_playlist);
  tcase_add_test (tc_m3u8, test_load_main_playlist_with_empty_lines);
  tcase_add_test (tc_m3u8, test_load_windows_main_playlist_with_empty_lines);
  tcase_add_test (tc_m3u8, test_on_demand_playlist);
  tcase_add_test (tc_m3u8, test_live_playlist);
  tcase_add_test (tc_m3u8, test_update_invalid_playlist);
  tcase_add_test (tc_m3u8, test_update_playlist);
  tcase_add_test (tc_m3u8, test_playlist_media_files);
  tcase_add_test (tc_m3u8, test_playlist_byte_range_media_files);
  tcase_add_test (tc_m3u8, test_get_next_fragment);
  tcase_add_test (tc_m3u8, test_get_current_position);
  tcase_add_test (tc_m3u8, test_get_duration);
  tcase_add_test (tc_m3u8, test_get_target_duration);
  tcase_add_test (tc_m3u8, test_get_stream_for_bitrate);
  tcase_add_test (tc_m3u8, test_get_streams_bitrates);
  tcase_add_test (tc_m3u8, test_seek);
  tcase_add_test (tc_m3u8, test_alternate_audio_playlist);
  tcase_add_test (tc_m3u8, test_subtitles_playlist);
  tcase_add_test (tc_m3u8, test_select_alternate);
  tcase_add_test (tc_m3u8, test_select_subs_alternate);
  tcase_add_test (tc_m3u8, test_simulation);
  tcase_add_test (tc_m3u8, test_playlist_with_doubles_duration);
  tcase_add_test (tc_m3u8, test_playlist_with_encription);

  suite_add_tcase (s, tc_adaptation);
  tcase_add_test (tc_m3u8, test_adaptation_add_fragments);
  tcase_add_test (tc_m3u8, test_adaptation_reset);
  tcase_add_test (tc_m3u8, test_adaptation_always_lowest);
  tcase_add_test (tc_m3u8, test_adaptation_always_highest);
  tcase_add_test (tc_m3u8, test_adaptation_fixed_bitrate);
  tcase_add_test (tc_m3u8, test_adaptation_bandwidth_estimation);
  return s;
}

int
main (int argc, char **argv)
{
  int nf;

  Suite *s = hlsdemux_suite ();
  SRunner *sr = srunner_create (s);

  gst_check_init (&argc, &argv);

  srunner_run_all (sr, CK_NORMAL);
  nf = srunner_ntests_failed (sr);
  srunner_free (sr);

  return nf;
}
