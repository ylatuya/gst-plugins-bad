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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <pthread.h>
#include <gmodule.h>
#include "gstjniutils.h"

static GModule *__java_module;
static jint (*get_created_java_vms) (JavaVM ** vmBuf, jsize bufLen,
    jsize * nVMs);
static jint (*create_java_vm) (JavaVM ** p_vm, JNIEnv ** p_env, void *vm_args);
static JavaVM *__java_vm;
static gboolean __started_java_vm = FALSE;
static gboolean __initialized = FALSE;
static pthread_key_t __current_jni_env;

#define CALL_TYPE_METHOD(_type, _name,  _jname, _retval)                                                 \
_type gst_amc_jni_call_##_name##_method (JNIEnv *env, jobject obj, jmethodID methodID, ...)                  \
  {                                                                                                      \
    _type ret;                                                                                           \
    va_list args;                                                                                        \
    va_start(args, methodID);                                                                            \
    ret = (*env)->Call##_jname##MethodV(env, obj, methodID, args);                                       \
    if ((*env)->ExceptionCheck (env)) {                                                                  \
      GST_ERROR ("Failed to call Java method");                                                          \
      (*env)->ExceptionClear (env);                                                                      \
      ret = _retval;                                                                                     \
    }                                                                                                    \
    va_end(args);                                                                                        \
    return (_type) ret;                                                                                  \
  }

CALL_TYPE_METHOD (gboolean, boolean, Boolean, FALSE)
    CALL_TYPE_METHOD (gint8, byte, Byte, G_MININT8)
    CALL_TYPE_METHOD (gshort, short, Short, G_MINSHORT)
CALL_TYPE_METHOD (gint, int, Int, G_MININT)
CALL_TYPE_METHOD (gchar, char, Char, 0)
CALL_TYPE_METHOD (glong, long, Long, G_MINLONG)
CALL_TYPE_METHOD (gfloat, float, Float, G_MINFLOAT)
CALL_TYPE_METHOD (gdouble, double, Double, G_MINDOUBLE)
CALL_TYPE_METHOD (jobject, object, Object, NULL)
#define GET_TYPE_FIELD(_type, _name, _jname, _retval)                           \
_type gst_amc_jni_get_##_name##_field (JNIEnv *env, jobject obj, jfieldID fieldID)  \
  {                                                                             \
    _type res;                                                                  \
                                                                                \
    res = (*env)->Get##_jname##Field(env, obj, fieldID);                        \
    if ((*env)->ExceptionCheck (env)) {                                         \
      GST_ERROR ("Failed to get Java field ");                                  \
      (*env)->ExceptionClear (env);                                             \
      res = _retval;                                                            \
    }                                                                           \
    return res;                                                                 \
  }
GET_TYPE_FIELD (gint, int, Int, G_MININT)
GET_TYPE_FIELD (glong, long, Long, G_MINLONG)


  jclass
gst_amc_jni_get_class (JNIEnv * env, const gchar * name)
{
  jclass tmp, ret = NULL;

  GST_DEBUG ("Retrieving Java class %s", name);

  tmp = (*env)->FindClass (env, name);
  if (!tmp) {
    ret = FALSE;
    (*env)->ExceptionClear (env);
    GST_ERROR ("Failed to get %s class", name);
    goto done;
  }

  ret = (*env)->NewGlobalRef (env, tmp);
  if (!ret) {
    (*env)->ExceptionClear (env);
    GST_ERROR ("Failed to get %s class global reference", name);
  }

done:
  if (tmp)
    (*env)->DeleteLocalRef (env, tmp);
  tmp = NULL;

  return ret;
}

jmethodID
gst_amc_jni_get_method (JNIEnv * env, jclass klass, const gchar * name,
    const gchar * signature)
{
  jmethodID ret;

  ret = (*env)->GetMethodID (env, klass, name, signature);
  if (!ret) {
    (*env)->ExceptionClear (env);
    GST_ERROR ("Failed to get method ID %s", name);
  }
  return ret;
}

jmethodID
gst_amc_jni_get_static_method (JNIEnv * env, jclass klass, const gchar * name,
    const gchar * signature)
{
  jmethodID ret;

  ret = (*env)->GetStaticMethodID (env, klass, name, signature);
  if (!ret) {
    (*env)->ExceptionClear (env);
    GST_ERROR ("Failed to get static method id %s", name);
  }
  return ret;
}

jfieldID
gst_amc_jni_get_field_id (JNIEnv * env, jclass klass, const gchar * name,
    const gchar * type)
{
  jfieldID ret;

  ret = (*env)->GetFieldID (env, klass, name, type);
  if (!ret) {
    (*env)->ExceptionClear (env);
    GST_ERROR ("Failed to get field ID %s", name);
  }
  return ret;
}

jobject
gst_amc_jni_new_object (JNIEnv * env, jclass klass, jmethodID constructor, ...)
{
  jobject tmp;
  va_list args;

  va_start (args, constructor);
  tmp = (*env)->NewObjectV (env, klass, constructor, args);
  va_end (args);

  if (!tmp) {
    (*env)->ExceptionClear (env);
    GST_ERROR ("Failed to create object");
    return NULL;
  }

  return gst_amc_jni_object_make_global (env, tmp);
}

jobject
gst_amc_jni_new_object_from_static (JNIEnv * env, jclass klass,
    jmethodID method, ...)
{
  jobject tmp;
  va_list args;

  va_start (args, method);
  tmp = (*env)->CallStaticObjectMethodV (env, klass, method, args);
  va_end (args);

  if ((*env)->ExceptionCheck (env) || !tmp) {
    (*env)->ExceptionClear (env);
    GST_ERROR ("Failed to create object from static method");
    return NULL;
  }

  return gst_amc_jni_object_make_global (env, tmp);
}

jobject
gst_amc_jni_object_make_global (JNIEnv * env, jobject object)
{
  jobject ret;

  ret = (*env)->NewGlobalRef (env, object);
  if (!ret) {
    GST_ERROR ("Failed to create global reference");
    (*env)->ExceptionClear (env);
  } else {
    gst_amc_jni_object_local_unref (env, object);
  }
  return ret;
}

jobject
gst_amc_jni_object_ref (JNIEnv * env, jobject object)
{
  jobject ret;

  ret = (*env)->NewGlobalRef (env, object);
  if (!ret) {
    GST_ERROR ("Failed to create global reference");
    (*env)->ExceptionClear (env);
  }
  return ret;
}

void
gst_amc_jni_object_unref (JNIEnv * env, jobject object)
{
  (*env)->DeleteGlobalRef (env, object);
}

void
gst_amc_jni_object_local_unref (JNIEnv * env, jobject object)
{
  (*env)->DeleteLocalRef (env, object);
}

jstring
gst_amc_jni_string_from_gchar (JNIEnv * env, const gchar * string)
{
  return (*env)->NewStringUTF (env, string);
}

gchar *
gst_amc_jni_string_to_gchar (JNIEnv * env, jstring string, gboolean release)
{
  const gchar *s = NULL;
  gchar *ret = NULL;

  s = (*env)->GetStringUTFChars (env, string, NULL);
  if (!s) {
    GST_ERROR ("Failed to convert string to UTF8");
    (*env)->ExceptionClear (env);
    return ret;
  }

  ret = g_strdup (s);
  (*env)->ReleaseStringUTFChars (env, string, s);

  if (release) {
    (*env)->DeleteLocalRef (env, string);
  }
  return ret;
}

gboolean
gst_amc_jni_call_void_method (JNIEnv * env, jobject obj, jmethodID methodID,
    ...)
{
  gboolean ret = TRUE;
  va_list args;
  va_start (args, methodID);

  (*env)->CallVoidMethodV (env, obj, methodID, args);
  if ((*env)->ExceptionCheck (env)) {
    GST_ERROR ("Failed to call Java method");
    (*env)->ExceptionClear (env);
    ret = FALSE;
  }
  va_end (args);
  return ret;
}

static JNIEnv *
gst_amc_jni_attach_current_thread (void)
{
  JNIEnv *env;
  JavaVMAttachArgs args;

  GST_DEBUG ("Attaching thread %p", g_thread_self ());
  args.version = JNI_VERSION_1_6;
  args.name = NULL;
  args.group = NULL;

  if ((*__java_vm)->AttachCurrentThread (__java_vm, &env, &args) < 0) {
    GST_ERROR ("Failed to attach current thread");
    return NULL;
  }

  return env;
}

static void
gst_amc_jni_detach_current_thread (void *env)
{
  GST_DEBUG ("Detaching thread %p", g_thread_self ());
  (*__java_vm)->DetachCurrentThread (__java_vm);
}

static gboolean
gst_amc_jni_initialize_java_vm (void)
{
  jsize n_vms;

  __java_module = g_module_open ("libdvm", G_MODULE_BIND_LOCAL);
  if (!__java_module)
    goto load_failed;

  if (!g_module_symbol (__java_module, "JNI_CreateJavaVM",
          (gpointer *) & create_java_vm))
    goto symbol_error;

  if (!g_module_symbol (__java_module, "JNI_GetCreatedJavaVMs",
          (gpointer *) & get_created_java_vms))
    goto symbol_error;

  n_vms = 0;
  if (get_created_java_vms (&__java_vm, 1, &n_vms) < 0)
    goto get_created_failed;

  if (n_vms > 0) {
    g_print ("Successfully got existing Java VM %p\n", __java_vm);
    GST_DEBUG ("Successfully got existing Java VM %p", __java_vm);
  } else {
    JNIEnv *env;
    JavaVMInitArgs vm_args;
    JavaVMOption options[4];

    options[0].optionString = "-verbose:jni";
    options[1].optionString = "-verbose:gc";
    options[2].optionString = "-Xcheck:jni";
    options[3].optionString = "-Xdebug";

    vm_args.version = JNI_VERSION_1_4;
    vm_args.options = options;
    vm_args.nOptions = 4;
    vm_args.ignoreUnrecognized = JNI_TRUE;
    if (create_java_vm (&__java_vm, &env, &vm_args) < 0)
      goto create_failed;
    g_print ("Successfully created Java VM %p\n", __java_vm);
    GST_DEBUG ("Successfully created Java VM %p", __java_vm);

    __started_java_vm = TRUE;
  }

  return __java_vm != NULL;

load_failed:
  {
    GST_ERROR ("Failed to load libdvm: %s", g_module_error ());
    return FALSE;
  }
symbol_error:
  {
    GST_ERROR ("Failed to locate required JNI symbols in libdvm: %s",
        g_module_error ());
    g_module_close (__java_module);
    __java_module = NULL;
    return FALSE;
  }
get_created_failed:
  {
    GST_ERROR ("Failed to get already created VMs");
    g_module_close (__java_module);
    __java_module = NULL;
    return FALSE;
  }
create_failed:
  {
    GST_ERROR ("Failed to create a Java VM");
    g_module_close (__java_module);
    __java_module = NULL;
    return FALSE;
  }
}

gboolean
gst_amc_jni_initialize (void)
{
  if (!__initialized) {
    pthread_key_create (&__current_jni_env, gst_amc_jni_detach_current_thread);
    __initialized = gst_amc_jni_initialize_java_vm ();
  }
  return __initialized;
}

JNIEnv *
gst_amc_jni_get_env (void)
{
  JNIEnv *env;

  if ((env = pthread_getspecific (__current_jni_env)) == NULL) {
    env = gst_amc_jni_attach_current_thread ();
    pthread_setspecific (__current_jni_env, env);
  }

  return env;
}

gboolean
gst_amc_jni_is_vm_started (void)
{
  return __started_java_vm;
}
