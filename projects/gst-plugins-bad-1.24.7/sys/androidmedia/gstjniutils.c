/*
 * Copyright (C) 2012, Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2013, Fluendo S.A.
 *   Author: Andoni Morales <amorales@fluendo.com>
 * Copyright (C) 2014, Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2014, Collabora Ltd.
 *   Author: Matthieu Bouron <matthieu.bouron@collabora.com>
 * Copyright (C) 2015, Sebastian Dröge <sebastian@centricular.com>
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

static GModule *java_module;
static jint (*get_created_java_vms) (JavaVM ** vmBuf, jsize bufLen,
    jsize * nVMs);
static jint (*create_java_vm) (JavaVM ** p_vm, JNIEnv ** p_env, void *vm_args);
static JavaVM *java_vm;
static gboolean started_java_vm = FALSE;
static pthread_key_t current_jni_env;
static jobject (*get_class_loader) (void);

gint
gst_amc_jni_get_android_level ()
{
  JNIEnv *env;
  gint ret = __ANDROID_API__;
  jfieldID sdkIntFieldID = NULL;

  env = gst_amc_jni_get_env ();

  jclass versionClass = (*env)->FindClass (env, "android/os/Build$VERSION");
  if (versionClass == NULL)
    goto done;

  sdkIntFieldID = (*env)->GetStaticFieldID (env, versionClass, "SDK_INT", "I");
  if (sdkIntFieldID == NULL)
    goto done;

  ret = (*env)->GetStaticIntField (env, versionClass, sdkIntFieldID);
done:
  return ret;
}

jclass
gst_amc_jni_get_class (JNIEnv * env, GError ** err, const gchar * name)
{
  jclass tmp, ret = NULL;

  GST_DEBUG ("Retrieving Java class %s", name);

  tmp = (*env)->FindClass (env, name);
  if ((*env)->ExceptionCheck (env) || !tmp) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_FAILED, "Failed to find class %s", name);
    goto done;
  }

  ret = (*env)->NewGlobalRef (env, tmp);
  if (!ret) {
    GST_ERROR ("Failed to get %s class global reference", name);
  }

done:
  if (tmp)
    (*env)->DeleteLocalRef (env, tmp);
  tmp = NULL;

  return ret;
}

jmethodID
gst_amc_jni_get_method_id (JNIEnv * env, GError ** err, jclass klass,
    const gchar * name, const gchar * signature)
{
  jmethodID ret;

  ret = (*env)->GetMethodID (env, klass, name, signature);
  if ((*env)->ExceptionCheck (env) || !ret) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_FAILED, "Failed to get method ID %s (%s)", name,
        signature);
  }
  return ret;
}

jmethodID
gst_amc_jni_get_static_method_id (JNIEnv * env, GError ** err, jclass klass,
    const gchar * name, const gchar * signature)
{
  jmethodID ret;

  ret = (*env)->GetStaticMethodID (env, klass, name, signature);
  if ((*env)->ExceptionCheck (env) || !ret) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_FAILED, "Failed to get static method ID %s (%s)",
        name, signature);
  }
  return ret;
}

jfieldID
gst_amc_jni_get_field_id (JNIEnv * env, GError ** err, jclass klass,
    const gchar * name, const gchar * type)
{
  jfieldID ret;

  ret = (*env)->GetFieldID (env, klass, name, type);
  if ((*env)->ExceptionCheck (env) || !ret) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_FAILED, "Failed to get field ID %s (%s)", name, type);
  }
  return ret;
}

jfieldID
gst_amc_jni_get_static_field_id (JNIEnv * env, GError ** err, jclass klass,
    const gchar * name, const gchar * type)
{
  jfieldID ret;

  ret = (*env)->GetStaticFieldID (env, klass, name, type);
  if ((*env)->ExceptionCheck (env) || !ret) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_FAILED, "Failed to get static field ID %s (%s)", name,
        type);
  }
  return ret;
}

jobject
gst_amc_jni_new_object (JNIEnv * env, GError ** err, gboolean global,
    jclass klass, jmethodID constructor, ...)
{
  jobject tmp;
  va_list args;

  va_start (args, constructor);
  tmp = (*env)->NewObjectV (env, klass, constructor, args);
  va_end (args);

  if ((*env)->ExceptionCheck (env) || !tmp) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_FAILED, "Failed to create object");
    return NULL;
  }

  if (global)
    return gst_amc_jni_object_make_global (env, tmp);
  else
    return tmp;
}

jobject
gst_amc_jni_new_object_from_static (JNIEnv * env, GError ** err,
    gboolean global, jclass klass, jmethodID method, ...)
{
  jobject tmp;
  va_list args;

  va_start (args, method);
  tmp = (*env)->CallStaticObjectMethodV (env, klass, method, args);
  va_end (args);

  if ((*env)->ExceptionCheck (env) || !tmp) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_FAILED, "Failed to create object");
    return NULL;
  }

  if (global)
    return gst_amc_jni_object_make_global (env, tmp);
  else
    return tmp;
}

jobject
gst_amc_jni_object_make_global (JNIEnv * env, jobject object)
{
  jobject ret;
  g_return_val_if_fail (env != NULL, NULL);
  g_return_val_if_fail (object != NULL, NULL);

  ret = (*env)->NewGlobalRef (env, object);
  if (!ret) {
    GST_ERROR ("Failed to create global reference");
  }
  gst_amc_jni_object_local_unref (env, object);

  return ret;
}

jobject
gst_amc_jni_object_ref (JNIEnv * env, jobject object)
{
  jobject ret;
  g_return_val_if_fail (env != NULL, NULL);
  g_return_val_if_fail (object != NULL, NULL);

  ret = (*env)->NewGlobalRef (env, object);
  if (!ret) {
    GST_ERROR ("Failed to create global reference");
  }
  return ret;
}

void
gst_amc_jni_object_unref (JNIEnv * env, jobject object)
{
  g_return_if_fail (env != NULL);
  g_return_if_fail (object != NULL);

  (*env)->DeleteGlobalRef (env, object);
}

void
gst_amc_jni_object_local_unref (JNIEnv * env, jobject object)
{
  g_return_if_fail (env != NULL);
  g_return_if_fail (object != NULL);

  (*env)->DeleteLocalRef (env, object);
}

jstring
gst_amc_jni_string_from_gchar (JNIEnv * env, GError ** err,
    gboolean global, const gchar * string)
{
  jstring tmp;

  tmp = (*env)->NewStringUTF (env, string);
  if ((*env)->ExceptionCheck (env)) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_FAILED, "Failed to call Java method");
    tmp = NULL;
  }

  if (global)
    return gst_amc_jni_object_make_global (env, tmp);
  else
    return tmp;
}

gchar *
gst_amc_jni_string_to_gchar (JNIEnv * env, jstring string, gboolean release)
{
  const gchar *s = NULL;
  gchar *ret = NULL;

  s = (*env)->GetStringUTFChars (env, string, NULL);
  if (!s) {
    GST_ERROR ("Failed to convert string to UTF8");
    goto done;
  }

  ret = g_strdup (s);
  (*env)->ReleaseStringUTFChars (env, string, s);

done:
  if (release) {
    (*env)->DeleteLocalRef (env, string);
  }
  return ret;
}

/* getExceptionSummary() and getStackTrace() taken from Android's
 *   platform/libnativehelper/JNIHelp.cpp
 * Modified to work with normal C strings and without C++.
 *
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Returns a human-readable summary of an exception object. The buffer will
 * be populated with the "binary" class name and, if present, the
 * exception message.
 */
static gchar *
getExceptionSummary (JNIEnv * env, jthrowable exception)
{
  GString *gs = g_string_new ("");
  jclass exceptionClass = NULL, classClass = NULL;
  jmethodID classGetNameMethod, getMessage;
  jstring classNameStr = NULL, messageStr = NULL;
  const char *classNameChars, *messageChars;

  /* get the name of the exception's class */
  exceptionClass = (*env)->GetObjectClass (env, exception);
  classClass = (*env)->GetObjectClass (env, exceptionClass);
  classGetNameMethod =
      (*env)->GetMethodID (env, classClass, "getName", "()Ljava/lang/String;");

  classNameStr =
      (jstring) (*env)->CallObjectMethod (env, exceptionClass,
      classGetNameMethod);

  if (classNameStr == NULL) {
    if ((*env)->ExceptionCheck (env))
      (*env)->ExceptionClear (env);
    g_string_append (gs, "<error getting class name>");
    goto done;
  }

  classNameChars = (*env)->GetStringUTFChars (env, classNameStr, NULL);
  if (classNameChars == NULL) {
    if ((*env)->ExceptionCheck (env))
      (*env)->ExceptionClear (env);
    g_string_append (gs, "<error getting class name UTF-8>");
    goto done;
  }

  g_string_append (gs, classNameChars);

  (*env)->ReleaseStringUTFChars (env, classNameStr, classNameChars);

  /* if the exception has a detail message, get that */
  getMessage =
      (*env)->GetMethodID (env, exceptionClass, "getMessage",
      "()Ljava/lang/String;");
  messageStr = (jstring) (*env)->CallObjectMethod (env, exception, getMessage);
  if (messageStr == NULL) {
    if ((*env)->ExceptionCheck (env))
      (*env)->ExceptionClear (env);
    goto done;
  }
  g_string_append (gs, ": ");

  messageChars = (*env)->GetStringUTFChars (env, messageStr, NULL);
  if (messageChars != NULL) {
    g_string_append (gs, messageChars);
    (*env)->ReleaseStringUTFChars (env, messageStr, messageChars);
  } else {
    if ((*env)->ExceptionCheck (env))
      (*env)->ExceptionClear (env);
    g_string_append (gs, "<error getting message>");
  }

done:
  if (exceptionClass)
    (*env)->DeleteLocalRef (env, exceptionClass);
  if (classClass)
    (*env)->DeleteLocalRef (env, classClass);
  if (classNameStr)
    (*env)->DeleteLocalRef (env, classNameStr);
  if (messageStr)
    (*env)->DeleteLocalRef (env, messageStr);

  return g_string_free (gs, FALSE);
}

/*
 * Returns an exception (with stack trace) as a string.
 */
static gchar *
getStackTrace (JNIEnv * env, jthrowable exception)
{
  GString *gs = g_string_new ("");
  jclass stringWriterClass = NULL, printWriterClass = NULL;
  jclass exceptionClass = NULL;
  jmethodID stringWriterCtor, stringWriterToStringMethod;
  jmethodID printWriterCtor, printStackTraceMethod;
  jobject stringWriter = NULL, printWriter = NULL;
  jstring messageStr = NULL;
  const char *utfChars;

  stringWriterClass = (*env)->FindClass (env, "java/io/StringWriter");

  if (stringWriterClass == NULL) {
    g_string_append (gs, "<error getting java.io.StringWriter class>");
    goto done;
  }

  stringWriterCtor =
      (*env)->GetMethodID (env, stringWriterClass, "<init>", "()V");
  stringWriterToStringMethod =
      (*env)->GetMethodID (env, stringWriterClass, "toString",
      "()Ljava/lang/String;");

  printWriterClass = (*env)->FindClass (env, "java/io/PrintWriter");
  if (printWriterClass == NULL) {
    g_string_append (gs, "<error getting java.io.PrintWriter class>");
    goto done;
  }

  printWriterCtor =
      (*env)->GetMethodID (env, printWriterClass, "<init>",
      "(Ljava/io/Writer;)V");
  stringWriter = (*env)->NewObject (env, stringWriterClass, stringWriterCtor);
  if (stringWriter == NULL) {
    if ((*env)->ExceptionCheck (env))
      (*env)->ExceptionClear (env);
    g_string_append (gs, "<error creating new StringWriter instance>");
    goto done;
  }

  printWriter =
      (*env)->NewObject (env, printWriterClass, printWriterCtor, stringWriter);
  if (printWriter == NULL) {
    if ((*env)->ExceptionCheck (env))
      (*env)->ExceptionClear (env);
    g_string_append (gs, "<error creating new PrintWriter instance>");
    goto done;
  }

  exceptionClass = (*env)->GetObjectClass (env, exception);
  printStackTraceMethod =
      (*env)->GetMethodID (env, exceptionClass, "printStackTrace",
      "(Ljava/io/PrintWriter;)V");
  (*env)->CallVoidMethod (env, exception, printStackTraceMethod, printWriter);
  if ((*env)->ExceptionCheck (env)) {
    (*env)->ExceptionClear (env);
    g_string_append (gs, "<exception while printing stack trace>");
    goto done;
  }

  messageStr = (jstring) (*env)->CallObjectMethod (env, stringWriter,
      stringWriterToStringMethod);
  if (messageStr == NULL) {
    if ((*env)->ExceptionCheck (env))
      (*env)->ExceptionClear (env);
    g_string_append (gs, "<failed to call StringWriter.toString()>");
    goto done;
  }

  utfChars = (*env)->GetStringUTFChars (env, messageStr, NULL);
  if (utfChars == NULL) {
    if ((*env)->ExceptionCheck (env))
      (*env)->ExceptionClear (env);
    g_string_append (gs, "<failed to get UTF chars for message>");
    goto done;
  }

  g_string_append (gs, utfChars);

  (*env)->ReleaseStringUTFChars (env, messageStr, utfChars);

done:
  if (stringWriterClass)
    (*env)->DeleteLocalRef (env, stringWriterClass);
  if (printWriterClass)
    (*env)->DeleteLocalRef (env, printWriterClass);
  if (exceptionClass)
    (*env)->DeleteLocalRef (env, exceptionClass);
  if (stringWriter)
    (*env)->DeleteLocalRef (env, stringWriter);
  if (printWriter)
    (*env)->DeleteLocalRef (env, printWriter);
  if (messageStr)
    (*env)->DeleteLocalRef (env, messageStr);

  return g_string_free (gs, FALSE);
}

static JNIEnv *
gst_amc_jni_attach_current_thread (void)
{
  JNIEnv *env;
  JavaVMAttachArgs args;
  gint ret;

  GST_DEBUG ("Attaching thread %p", g_thread_self ());
  args.version = JNI_VERSION_1_6;
  args.name = NULL;
  args.group = NULL;

  if ((ret = (*java_vm)->AttachCurrentThread (java_vm, &env, &args)) != JNI_OK) {
    GST_ERROR ("Failed to attach current thread: %d", ret);
    return NULL;
  }

  return env;
}

static void
gst_amc_jni_detach_current_thread (void *env)
{
  gint ret;

  GST_DEBUG ("Detaching thread %p", g_thread_self ());
  if ((ret = (*java_vm)->DetachCurrentThread (java_vm)) != JNI_OK) {
    GST_DEBUG ("Failed to detach current thread: %d", ret);
  }
}

static JavaVM *
get_application_java_vm (void)
{
  GModule *module = NULL;
  JavaVM *(*get_java_vm) (void);
  JavaVM *vm = NULL;

  module = g_module_open (NULL, G_MODULE_BIND_LOCAL);

  if (!module) {
    return NULL;
  }

  if (g_module_symbol (module, "gst_android_get_java_vm",
          (gpointer *) & get_java_vm) && get_java_vm) {
    vm = get_java_vm ();
  }

  g_module_close (module);

  return vm;
}

static gboolean
check_nativehelper (void)
{
  GModule *module;
  void **jni_invocation = NULL;
  gboolean ret = FALSE;

  module = g_module_open (NULL, G_MODULE_BIND_LOCAL);
  if (!module)
    return ret;

  /* Check if libnativehelper is loaded in the process and if
   * it has these awful wrappers for JNI_CreateJavaVM and
   * JNI_GetCreatedJavaVMs that crash the app if you don't
   * create a JniInvocation instance first. If it isn't we
   * just fail here and don't initialize anything.
   * See this code for reference:
   * https://android.googlesource.com/platform/libnativehelper/+/master/JniInvocation.cpp
   */
  if (!g_module_symbol (module, "_ZN13JniInvocation15jni_invocation_E",
          (gpointer *) & jni_invocation)) {
    ret = TRUE;
  } else {
    ret = (jni_invocation != NULL && *jni_invocation != NULL);
  }

  g_module_close (module);

  return ret;
}

static gboolean
load_java_module (const gchar * name)
{
  java_module = g_module_open (name, G_MODULE_BIND_LOCAL);
  if (!java_module)
    goto load_failed;

  if (!g_module_symbol (java_module, "JNI_CreateJavaVM",
          (gpointer *) & create_java_vm)) {
    GST_ERROR ("Could not find 'JNI_CreateJavaVM' in '%s': %s",
        GST_STR_NULL (name), g_module_error ());
    create_java_vm = NULL;
  }

  if (!g_module_symbol (java_module, "JNI_GetCreatedJavaVMs",
          (gpointer *) & get_created_java_vms))
    goto symbol_error;

  return TRUE;

load_failed:
  {
    GST_ERROR ("Failed to load Java module '%s': %s", GST_STR_NULL (name),
        g_module_error ());
    return FALSE;
  }
symbol_error:
  {
    GST_ERROR ("Failed to locate required JNI symbols in '%s': %s",
        GST_STR_NULL (name), g_module_error ());
    g_module_close (java_module);
    java_module = NULL;
    return FALSE;
  }
}

static gboolean
check_application_class_loader (void)
{
  gboolean ret = TRUE;
  GModule *module = NULL;

  module = g_module_open (NULL, G_MODULE_BIND_LOCAL);
  if (!module) {
    return FALSE;
  }
  if (!g_module_symbol (module, "gst_android_get_application_class_loader",
          (gpointer *) & get_class_loader)) {
    ret = FALSE;
  }

  g_module_close (module);

  return ret;
}

static gboolean
initialize_classes (void)
{
  if (!check_application_class_loader ()) {
    GST_ERROR ("Could not find application class loader provider");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amc_jni_initialize_java_vm (void)
{
  jsize n_vms;
  gint ret;

  if (java_vm) {
    GST_DEBUG ("Java VM already provided by the application");
    return initialize_classes ();
  }

  java_vm = get_application_java_vm ();
  if (java_vm) {
    GST_DEBUG ("Java VM successfully requested from the application");
    return initialize_classes ();
  }

  /* Returns TRUE if we can safely
   * a) get the current VMs and
   * b) start a VM if none is started yet
   *
   * FIXME: On Android >= 4.4 we won't be able to safely start a
   * VM on our own without using private C++ API!
   */
  if (!check_nativehelper ()) {
    GST_ERROR ("Can't safely check for VMs or start a VM");
    return FALSE;
  }

  if (!load_java_module (NULL)) {
    if (!load_java_module ("libdvm"))
      return FALSE;
  }

  n_vms = 0;
  if ((ret = get_created_java_vms (&java_vm, 1, &n_vms)) != JNI_OK)
    goto get_created_failed;

  if (n_vms > 0) {
    GST_DEBUG ("Successfully got existing Java VM %p", java_vm);
  } else if (create_java_vm) {
    JNIEnv *env;
    JavaVMInitArgs vm_args;
    JavaVMOption options[4];

    GST_DEBUG ("Found no existing Java VM, trying to start one");

    options[0].optionString = "-verbose:jni";
    options[1].optionString = "-verbose:gc";
    options[2].optionString = "-Xcheck:jni";
    options[3].optionString = "-Xdebug";

    vm_args.version = JNI_VERSION_1_4;
    vm_args.options = options;
    vm_args.nOptions = 4;
    vm_args.ignoreUnrecognized = JNI_TRUE;
    if ((ret = create_java_vm (&java_vm, &env, &vm_args)) != JNI_OK)
      goto create_failed;
    GST_DEBUG ("Successfully created Java VM %p", java_vm);

    started_java_vm = TRUE;
  } else {
    GST_ERROR ("JNI_CreateJavaVM not available");
    java_vm = NULL;
  }

  if (java_vm == NULL)
    return FALSE;

  return initialize_classes ();

get_created_failed:
  {
    GST_ERROR ("Failed to get already created VMs: %d", ret);
    g_module_close (java_module);
    java_module = NULL;
    return FALSE;
  }
create_failed:
  {
    GST_ERROR ("Failed to create a Java VM: %d", ret);
    g_module_close (java_module);
    java_module = NULL;
    return FALSE;
  }
}

static void
gst_amc_jni_set_error_string (JNIEnv * env, GError ** err, GQuark domain,
    gint code, const gchar * message)
{
  jthrowable exception;

  if (!err) {
    if ((*env)->ExceptionCheck (env))
      (*env)->ExceptionClear (env);
    return;
  }

  if ((*env)->ExceptionCheck (env)) {
    if ((exception = (*env)->ExceptionOccurred (env))) {
      gchar *exception_description, *exception_stacktrace;

      /* Clear exception so that we can call Java methods again */
      (*env)->ExceptionClear (env);

      exception_description = getExceptionSummary (env, exception);
      exception_stacktrace = getStackTrace (env, exception);
      g_set_error (err, domain, code, "%s: %s\n%s", message,
          exception_description, exception_stacktrace);
      g_free (exception_description);
      g_free (exception_stacktrace);

      (*env)->DeleteLocalRef (env, exception);
    } else {
      (*env)->ExceptionClear (env);
      g_set_error (err, domain, code, "%s", message);
    }
  } else {
    g_set_error (err, domain, code, "%s", message);
  }
}

G_GNUC_PRINTF (5, 6)
     void gst_amc_jni_set_error (JNIEnv * env, GError ** err, GQuark domain,
    gint code, const gchar * format, ...)
{
  gchar *message;
  va_list var_args;

  va_start (var_args, format);
  message = g_strdup_vprintf (format, var_args);
  va_end (var_args);

  gst_amc_jni_set_error_string (env, err, domain, code, message);

  g_free (message);
}

static gpointer
gst_amc_jni_initialize_internal (gpointer data)
{
  pthread_key_create (&current_jni_env, gst_amc_jni_detach_current_thread);
  return gst_amc_jni_initialize_java_vm ()? GINT_TO_POINTER (1) : NULL;
}

/* Allow the application to set the Java VM */
void
gst_amc_jni_set_java_vm (JavaVM * vm)
{
  GST_DEBUG ("Application provides Java VM %p", vm);
  java_vm = vm;
}

gboolean
gst_amc_jni_initialize (void)
{
  GOnce once = G_ONCE_INIT;

  g_once (&once, gst_amc_jni_initialize_internal, NULL);
  return once.retval != NULL;
}

JNIEnv *
gst_amc_jni_get_env (void)
{
  JNIEnv *env;

  if ((env = pthread_getspecific (current_jni_env)) == NULL) {
    env = gst_amc_jni_attach_current_thread ();
    pthread_setspecific (current_jni_env, env);
  }

  return env;
}

gboolean
gst_amc_jni_is_vm_started (void)
{
  return started_java_vm;
}

jclass
gst_amc_jni_get_application_class (JNIEnv * env, const gchar * name,
    GError ** err)
{
  jclass tmp = NULL;
  jclass class = NULL;
  jstring name_jstr = NULL;

  jobject class_loader = NULL;
  jclass class_loader_cls = NULL;
  jmethodID load_class_id = 0;

  GST_LOG ("attempting to retrieve class %s", name);

  if (!get_class_loader) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Could not retrieve application class loader function");
    goto done;
  }

  class_loader = get_class_loader ();
  if (!class_loader) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Could not retrieve application class loader");
    goto done;
  }

  class_loader_cls = (*env)->GetObjectClass (env, class_loader);
  if (!class_loader_cls) {
    g_set_error (err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,
        "Could not retrieve application class loader java class");
    goto done;
  }

  load_class_id =
      gst_amc_jni_get_method_id (env, err, class_loader_cls, "loadClass",
      "(Ljava/lang/String;)Ljava/lang/Class;");
  if (!load_class_id) {
    goto done;
  }

  name_jstr = gst_amc_jni_string_from_gchar (env, err, FALSE, name);
  if (!name_jstr) {
    goto done;
  }

  if (gst_amc_jni_call_object_method (env, err, class_loader,
          load_class_id, &tmp, name_jstr)) {
    class = gst_amc_jni_object_make_global (env, tmp);
  }

done:
  gst_amc_jni_object_local_unref (env, name_jstr);
  gst_amc_jni_object_local_unref (env, class_loader_cls);

  return class;
}

#define CALL_STATIC_TYPE_METHOD(_type, _name,  _jname)                                                     \
gboolean gst_amc_jni_call_static_##_name##_method (JNIEnv *env, GError ** err, jclass klass, jmethodID methodID, _type * value, ...)   \
  {                                                                                                          \
    gboolean ret = TRUE;                                                                                     \
    va_list args;                                                                                            \
    va_start(args, value);                                                                                \
    *value = (*env)->CallStatic##_jname##MethodV(env, klass, methodID, args);                                \
    if ((*env)->ExceptionCheck (env)) {                                                                      \
      gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,                               \
          "Failed to call static Java method");                                                         \
      ret = FALSE;                                                                                           \
    }                                                                                                        \
    va_end(args);                                                                                            \
    return ret;                                                                                              \
  }

CALL_STATIC_TYPE_METHOD (gboolean, boolean, Boolean);
CALL_STATIC_TYPE_METHOD (gint8, byte, Byte);
CALL_STATIC_TYPE_METHOD (gshort, short, Short);
CALL_STATIC_TYPE_METHOD (gint, int, Int);
CALL_STATIC_TYPE_METHOD (gchar, char, Char);
CALL_STATIC_TYPE_METHOD (gint64, long, Long);
CALL_STATIC_TYPE_METHOD (gfloat, float, Float);
CALL_STATIC_TYPE_METHOD (gdouble, double, Double);
CALL_STATIC_TYPE_METHOD (jobject, object, Object);

gboolean
gst_amc_jni_call_static_void_method (JNIEnv * env, GError ** err, jclass klass,
    jmethodID methodID, ...)
{
  gboolean ret = TRUE;
  va_list args;
  va_start (args, methodID);

  (*env)->CallStaticVoidMethodV (env, klass, methodID, args);
  if ((*env)->ExceptionCheck (env)) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_FAILED, "Failed to call static Java method");
    ret = FALSE;
  }
  va_end (args);
  return ret;
}

#define CALL_TYPE_METHOD(_type, _name,  _jname)                                                              \
gboolean gst_amc_jni_call_##_name##_method (JNIEnv *env, GError ** err, jobject obj, jmethodID methodID, _type *value, ...)   \
  {                                                                                                          \
    gboolean ret = TRUE;                                                                                     \
    va_list args;                                                                                            \
    va_start(args, value);                                                                                \
    *value = (*env)->Call##_jname##MethodV(env, obj, methodID, args);                                        \
    if ((*env)->ExceptionCheck (env)) {                                                                      \
      gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,                               \
          "Failed to call Java method");                                                                \
      ret = FALSE;                                                                                           \
    }                                                                                                        \
    va_end(args);                                                                                            \
    return ret;                                                                                              \
  }

CALL_TYPE_METHOD (gboolean, boolean, Boolean);
CALL_TYPE_METHOD (gint8, byte, Byte);
CALL_TYPE_METHOD (gshort, short, Short);
CALL_TYPE_METHOD (gint, int, Int);
CALL_TYPE_METHOD (gchar, char, Char);
CALL_TYPE_METHOD (gint64, long, Long);
CALL_TYPE_METHOD (gfloat, float, Float);
CALL_TYPE_METHOD (gdouble, double, Double);
CALL_TYPE_METHOD (jobject, object, Object);

gboolean
gst_amc_jni_call_void_method (JNIEnv * env, GError ** err, jobject obj,
    jmethodID methodID, ...)
{
  gboolean ret = TRUE;
  va_list args;
  va_start (args, methodID);

  (*env)->CallVoidMethodV (env, obj, methodID, args);
  if ((*env)->ExceptionCheck (env)) {
    gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR,
        GST_LIBRARY_ERROR_FAILED, "Failed to call Java method");
    ret = FALSE;
  }
  va_end (args);
  return ret;
}

#define GET_TYPE_FIELD(_type, _name, _jname)                                               \
gboolean gst_amc_jni_get_##_name##_field (JNIEnv *env, GError ** err, jobject obj, jfieldID fieldID, _type *value)   \
  {                                                                                                 \
    gboolean ret = TRUE;                                                                            \
                                                                                                    \
    *value = (*env)->Get##_jname##Field(env, obj, fieldID);                                         \
    if ((*env)->ExceptionCheck (env)) {                                                             \
      gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,                      \
          "Failed to get Java field");                                                         \
      ret = FALSE;                                                                                  \
    }                                                                                               \
    return ret;                                                                                     \
  }

GET_TYPE_FIELD (gboolean, boolean, Boolean);
GET_TYPE_FIELD (gint8, byte, Byte);
GET_TYPE_FIELD (gshort, short, Short);
GET_TYPE_FIELD (gint, int, Int);
GET_TYPE_FIELD (gchar, char, Char);
GET_TYPE_FIELD (gint64, long, Long);
GET_TYPE_FIELD (gfloat, float, Float);
GET_TYPE_FIELD (gdouble, double, Double);
GET_TYPE_FIELD (jobject, object, Object);

#define GET_STATIC_TYPE_FIELD(_type, _name, _jname)                                               \
gboolean gst_amc_jni_get_static_##_name##_field (JNIEnv *env, GError ** err, jclass klass, jfieldID fieldID, _type *value)   \
  {                                                                                                 \
    gboolean ret = TRUE;                                                                            \
                                                                                                    \
    *value = (*env)->GetStatic##_jname##Field(env, klass, fieldID);                                 \
    if ((*env)->ExceptionCheck (env)) {                                                             \
      gst_amc_jni_set_error (env, err, GST_LIBRARY_ERROR, GST_LIBRARY_ERROR_FAILED,                      \
          "Failed to get static Java field");                                                  \
      ret = FALSE;                                                                                  \
    }                                                                                               \
    return ret;                                                                                     \
  }

GET_STATIC_TYPE_FIELD (gboolean, boolean, Boolean);
GET_STATIC_TYPE_FIELD (gint8, byte, Byte);
GET_STATIC_TYPE_FIELD (gshort, short, Short);
GET_STATIC_TYPE_FIELD (gint, int, Int);
GET_STATIC_TYPE_FIELD (gchar, char, Char);
GET_STATIC_TYPE_FIELD (gint64, long, Long);
GET_STATIC_TYPE_FIELD (gfloat, float, Float);
GET_STATIC_TYPE_FIELD (gdouble, double, Double);
GET_STATIC_TYPE_FIELD (jobject, object, Object);
