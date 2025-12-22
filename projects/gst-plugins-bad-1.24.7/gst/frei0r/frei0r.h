/** @mainpage frei0r - a minimalistic plugin API for video effects
 *
 * @section sec_intro Introduction
 *
 * This is frei0r - a minimalistic plugin API for video effects.
 *
 * The main emphasis is on simplicity - there are many different applications
 * that use video effects, and they all have different requirements regarding
 * their internal plugin API. And that's why frei0r does not try to be a
 * one-in-all general video plugin API, but instead an API for the most
 * common video effects: simple filters, sources and mixers that can be
 * controlled by parameters.
 *
 * It's our hope that this way these simple effects can be shared between
 * many applications, avoiding their reimplementation by different
 * projects.
 *
 * On the other hand, this is not meant as a competing standard to
 * more ambitious efforts that try to satisfy the needs of many different
 * applications and more complex effects.
 *
 *
 * @section sec_overview Overview
 *
 * If you are new to frei0r, the best thing is probably to have
 * a look at the <a href="frei0r_8h-source.html">frei0r header</a>,
 * which is quite simple.
 *
 * After that, you might want to look at the 
 * <a href="frei0r_8h.html">frei0r functions</a> in more detail.
 *
 * When developing a new frei0r effect, you have to choose
 *  - which effect type to use (\ref PLUGIN_TYPE),
 *  - which color model to use (\ref COLOR_MODEL), and
 *  - which parameter types (\ref PARAM_TYPE) your effect will support.
 *
 * To round things up, you should decide whether your effect should have
 * an associated icon (\ref icons), and where it will be installed
 * (\ref pluglocations).
 *
 * @section sec_changes Changes
 *
 * @subsection sec_changes_1_1_1_2 From frei0r 1.1 to frei0r 1.2
 *   - make <vendor> in plugin path optional
 *   - added section on FREI0R_PATH environment variable
 *   - added requirement to initialize all parameters in f0r_construct()
 *
 * @subsection sec_changes_1_0_1_1 From frei0r 1.0 to frei0r 1.1
 *
 *   - added specifications for plugin locations
 *   - added specifications for frei0r icons
 *   - added RGBA8888 color model
 *   - added packed32 color model
 *   - added better specification of color models
 *   - added string type
 *   - added bounds to resolution (8 <= width, height <= 2048)
 *   - width and height must be an integer multiple of 8
 *   - frame data must be 16 byte aligned
 *   - improved update specification (must not change parameters,
 *      must restore fpu state)
 *   - added note for applications to ignore effects with unknown fields
 *   - added new plugin types mixer2 and mixer3
 *   - added section about \ref concurrency
 */


/**
 * \addtogroup pluglocations Plugin Locations
 * @section sec_pluglocations Plugin Locations
 *
 * For Unix platforms there are rules for the location of frei0r plugins.
 *
 * frei0r 1.x plugin files should be located in
 *
 * - (1) /usr/lib/frei0r-1/\<vendor\>
 * - (2) /usr/local/lib/frei0r-1/\<vendor\>
 * - (3) $HOME/.frei0r-1/lib/\<vendor\>
 *
 * Examples:
 *
 * - /usr/lib/frei0r-1/mob/flippo.so
 * - /usr/lib/frei0r-1/drone/flippo.so
 * - /usr/local/lib/frei0r-1/gephex/coma/invert0r.so
 * - /home/martin/.frei0r-1/lib/martin/test.so
 *
 * Like in these examples plugins should be placed in "vendor" subdirs
 * to reduce name clashes. However, <vendor> is optional and may be left blank.
 *
 * @subsection sec_order Plugin Loading Order
 *
 * The application shall load plugins in the following order: 3, 2, 1.
 * If a name clash occurs (two or more frei0r plugins with identical
 * effect name), the plugins in directory 3 have precedence over plugins
 * in directory 2, and those in directory 2 have precedence over plugins
 * in directory 1.
 *
 * This makes it possible for users to "override" effects that are
 * installed in system wide directories by placing plugins in their
 * home directory.
 *
 * The order of loading plugins inside each of the directories 
 * 1, 2, and 3 is not defined.
 *
 * @subsection sec_path FREI0R_PATH Environment Variable
 *
 * If the environment variable FREI0R_PATH is defined, then it shall be
 * considered a colon separated list of directories which replaces the
 * default list.
 *
 * For example:
 *
 * FREI0R_PATH=/home/foo/frei0r-plugins:/usr/lib/frei0r-1:/etc/frei0r
 */

/**
 *\addtogroup icons Icons for frei0r effects
 * @section sec_icons Icons for frei0r effects
 *
 * Each frei0r effect can have an associated icon.
 *
 * @subsection sec_icon_format Icon Format
 *
 * The format of frei0r icons must be png.
 * Recommended resolution is 64x64.
 * The icon filename of an effect with effect name "frei0r"
 * must be "frei0r.png".
 *
 * @subsection sec_icon_location Icon location
 *
 * The exact location where the application should look for the
 * plugin is platform dependent.
 *
 * For Windows platforms, the icon should be at the same place as
 * the plugin containing the effect.
 *
 * For Unix platforms, the following mapping from plugin location
 * to icon location must be used:
 *
 * Let \<plugin_path\>/\<plugin\> be a frei0r plugin with name \<effect_name\>.
 * Then the corresponding icon (if any) shall be located in
 * \<icon_path\>/\<effect_name\>.png.
 * \<icon_path\> can be obtained in the following way:
 *
 * @verbatim
  <plugin_path>                   |     <icon_path>                   
 ----------------------------------------------------------------------------
 $HOME/.frei0r-1/lib/<vendor>     | $HOME/.frei0r-1/icons/<vendor>    
 /usr/local/lib/frei0r-1/<vendor> | /usr/local/share/frei0r-1/icons/<vendor>
 /usr/lib/frei0r-1/<vendor>       | /usr/share/frei0r-1/icons/<vendor>
          *                       | <plugin_path>
 @endverbatim
 *
 * (The wildcard '*' stands for any other plugin_path)
 *
 * For other platforms, no location is defined. We recommend to use the
 * plugin path where possible.
 */

/**
 * \addtogroup concurrency Concurrency
 * @section sec_concurrency Concurrency
 *
 * - \ref f0r_init
 * - \ref f0r_deinit
 *
 * These methods must not be called more than once. It is obvious that no
 * concurrent calls are allowed.
 *
 *
 * - \ref f0r_get_plugin_info
 * - \ref f0r_get_param_info
 * - \ref f0r_construct
 * - \ref f0r_destruct
 *
 * Concurrent calls of these functions are allowed.
 *
 *
 * - \ref f0r_set_param_value
 * - \ref f0r_get_param_value
 * - \ref f0r_update
 * - \ref f0r_update2
 *
 * If a thread is in one of these methods its allowed for another thread to
 * enter one of theses methods for a different effect instance. But for one
 * effect instance only one thread is allowed to execute any of these methods. 
 */



/** \file
 * \brief This file defines the frei0r api, version 1.2.
 *
 * A conforming plugin must implement and export all functions declared in
 * this header.
 *
 * A conforming application must accept only those plugins which use
 * allowed values for the described fields.
 */

#ifndef INCLUDED_FREI0R_H
#define INCLUDED_FREI0R_H

#include <glib.h>

/**
 * The frei0r API major version
 */
#define FREI0R_MAJOR_VERSION 1

/**
 * The frei0r API minor version
 */
#define FREI0R_MINOR_VERSION 2

//---------------------------------------------------------------------------

/**
 * f0r_init() is called once when the plugin is loaded by the application.
 * \see f0r_deinit
 */
int f0r_init();

/**
 * f0r_deinit is called once when the plugin is unloaded by the application.
 * \see f0r_init
 */
void f0r_deinit();

//---------------------------------------------------------------------------

/** \addtogroup PLUGIN_TYPE Type of the Plugin
 * These defines determine whether the plugin is a
 * source, a filter or one of the two mixer types
 *  @{
 */

/** one input and one output */
#define F0R_PLUGIN_TYPE_FILTER 0
/** just one output */
#define F0R_PLUGIN_TYPE_SOURCE 1
/** two inputs and one output */
#define F0R_PLUGIN_TYPE_MIXER2 2
/** three inputs and one output */
#define F0R_PLUGIN_TYPE_MIXER3 3

/** @} */

//---------------------------------------------------------------------------

/** \addtogroup COLOR_MODEL Color Models
 * List of supported color models.
 *
 * Note: the color models are endian independent, because the
 * color components are defined by their position in memory, not
 * by their significance in an uint32_t value.
 * 
 * For effects that work on the color components,
 * RGBA8888 is the recommended color model for frei0r-1.2 effects.
 * For effects that only work on pixels, PACKED32 is the recommended
 * color model since it helps the application to avoid unnecessary
 * color conversions.
 *
 * Effects can choose an appropriate color model, applications must support
 * all color models and do conversions if necessary. Source effects
 * must not use the PACKED32 color model because the application must know
 * in which color model the created framebuffers are represented.
 *
 * For each color model, a frame consists of width*height pixels which
 * are stored row-wise and consecutively in memory. The size of a pixel is
 * 4 bytes. There is no extra pitch parameter
 * (i.e. the pitch is simply width*4).
 *
 * The following additional constraints must be honored:
 *   - The top-most line of a frame is stored first in memory.
 *   - A frame must be aligned to a 16 byte border in memory.
 *   - The width and height of a frame must be positive
 *   - The width and height of a frame must be integer multiples of 8
 *
 * These constraints make sure that each line is stored at an address aligned
 * to 16 byte.
 */
/*@{*/
/**
 * In BGRA8888, each pixel is represented by 4 consecutive
 * unsigned bytes, where the first byte value represents
 * the blue, the second the green, and the third the red color
 * component of the pixel. The last value represents the
 * alpha value.
 */
#define F0R_COLOR_MODEL_BGRA8888 0

/**
 * In RGBA8888, each pixel is represented by 4 consecutive
 * unsigned bytes, where the first byte value represents
 * the red, the second the green, and the third the blue color
 * component of the pixel. The last value represents the
 * alpha value.
 */
#define F0R_COLOR_MODEL_RGBA8888 1

/**
 * In PACKED32, each pixel is represented by 4 consecutive
 * bytes, but it is not defined how the color components are
 * stored. The true color format could be RGBA8888,
 * BGRA8888, a packed 32 bit YUV format, or any other
 * color format that stores pixels in 32 bit.
 *
 * This is useful for effects that don't work on color but
 * only on pixels (for example a mirror effect).
 *
 * Note that source effects must not use this color model.
 */
#define F0R_COLOR_MODEL_PACKED32 2
/*@}*/

/**
 * The f0r_plugin_info_t structure is filled in by the plugin
 * to tell the application about its name, type, number of parameters,
 * and version. 
 *
 * An application should ignore (i.e. not use) frei0r effects that
 * have unknown values in the plugin_type or color_model field.
 * It should also ignore effects with a too high frei0r_version.
 *
 * This is necessary to be able to extend the frei0r spec (e.g.
 * by adding new color models or plugin types) in a way that does not
 * result in crashes when loading effects that make use of these
 * extensions into an older application.
 *
 * All strings are unicode, 0-terminated, and the encoding is utf-8.
 */
typedef struct f0r_plugin_info
{
  const char* name;    /**< The (short) name of the plugin                   */
  const char* author;  /**< The plugin author                                */
  /** The plugin type
   * \see PLUGIN_TYPE
   */
  int plugin_type;    
  int color_model;     /**< The color model used                             */
  int frei0r_version;  /**< The frei0r major version this plugin is built for*/
  int major_version;   /**< The major version of the plugin                  */
  int minor_version;   /**< The minor version of the plugin                  */
  int num_params;      /**< The number of parameters of the plugin           */
  const char* explanation; /**< An optional explanation string               */
} f0r_plugin_info_t;


/**
 * Is called once after init. The plugin has to fill in the values in info.
 *
 * \param info Pointer to an info struct allocated by the application.
 */
void f0r_get_plugin_info(f0r_plugin_info_t* info);

//---------------------------------------------------------------------------

/** \addtogroup PARAM_TYPE Parameter Types
 *
 *  @{
 */


/**
 * Parameter type for boolean values
 * \see f0r_param_bool
 */
#define F0R_PARAM_BOOL      0

/**
 * Parameter type for doubles
 * \see f0r_param_double
 */
#define F0R_PARAM_DOUBLE    1

/**
 * Parameter type for color
 * \see f0r_param_color
 */
#define F0R_PARAM_COLOR     2
/**
 * Parameter type for position
 * \see f0r_param_position
 */
#define F0R_PARAM_POSITION  3

/**
 * Parameter type for string
 * \see f0r_param_string
 */
#define F0R_PARAM_STRING  4

/**
 * The boolean type. The allowed range of values is [0, 1].
 * [0, 0.5[ is mapped to false and [0.5, 1] is mapped to true.
 */
typedef double f0r_param_bool;

/**
 * The double type. The allowed range of values is [0, 1].
 */
typedef double f0r_param_double;

/**
 * The color type. All three color components are in the range [0, 1].
 */
typedef struct f0r_param_color
{
  float r; /**< red color component */
  float g; /**< green color component */
  float b; /**< blue color component */
} f0r_param_color_t;

/**
 * The position type. Both position coordinates are in the range [0, 1].
 */
typedef struct f0r_param_position
{
  double x; /**< x coordinate */
  double y; /**< y coordinate */
} f0r_param_position_t;


/**
 * The string type. 
 * Zero terminated array of 8-bit values in utf-8 encoding
 */
typedef char f0r_param_string;

/**  @} */


/**
 * Similar to f0r_plugin_info_t, this structure is filled by the plugin
 * for every parameter.
 *
 * All strings are unicode, 0-terminated, and the encoding is utf-8.
 */
typedef struct f0r_param_info
{
  const char* name;         /**<The (short) name of the param */
  int type;                 /**<The type (see the F0R_PARAM_* defines) */
  const char* explanation;  /**<Optional explanation (can be 0) */
} f0r_param_info_t;

/**
 * f0r_get_param_info is called by the application to query the type of
 * each parameter.
 *
 * \param info is allocated by the application and filled by the plugin
 * \param param_index the index of the parameter to be queried (from 0 to
 *   num_params-1)
 */
void f0r_get_param_info(f0r_param_info_t* info, int param_index);

//---------------------------------------------------------------------------

/**
 * Transparent instance pointer of the frei0r effect.
 */
typedef void* f0r_instance_t;

/**
 * Constructor for effect instances. The plugin returns a pointer to
 * its internal instance structure.
 *
 * The resolution must be an integer multiple of 8,
 * must be greater than 0 and be at most 2048 in both dimensions.
 * The plugin must set default values for all parameters in this function.
 *
 * \param width The x-resolution of the processed video frames
 * \param height The y-resolution of the processed video frames
 * \returns 0 on failure or a pointer != 0 on success
 *
 * \see f0r_destruct
 */
f0r_instance_t f0r_construct(unsigned int width, unsigned int height);

/**
 * Destroys an effect instance.
 *
 * \param instance The pointer to the plugins internal instance structure.
 *
 * \see f0r_construct
 */
void f0r_destruct(f0r_instance_t instance);

//---------------------------------------------------------------------------

/**
 * Transparent parameter handle.
 */
typedef void* f0r_param_t;

/**
 * This function allows the application to set the parameter values of an
 * effect instance. Validity of the parameter pointer is handled by the
 * application thus the data must be copied by the effect.
 *
 * Furthermore, if d an update event/signal is needed in a host
 * application to notice when parameters have changed, this should be
 * implemented inside its own update() call. The host application
 * would presumably need to store the current value as well to see if
 * it changes; to make this thread safe, it should store a copy of the
 * current value in a struct which uses instance as a key.
 *
 * \param instance the effect instance
 * \param param pointer to the parameter value
 * \param param_index index of the parameter
 *
 * \see f0r_get_param_value
 */
void f0r_set_param_value(f0r_instance_t instance, 
			 f0r_param_t param, int param_index);

/**
 * This function allows the application to query the parameter values of an
 * effect instance.
 *
 * \param instance the effect instance
 * \param param pointer to the parameter value
 * \param param_index index of the parameter
 *
  * \see f0r_set_param_value
 */
void f0r_get_param_value(f0r_instance_t instance,
			 f0r_param_t param, int param_index);

//---------------------------------------------------------------------------

/**
 * This is where the core effect processing happens. The application calls it
 * after it has set the necessary parameter values.
 * inframe and outframe must be aligned to an integer multiple of 16 bytes
 * in memory.
 *
 * This funcition should not alter the parameters of the effect in any
 * way (\ref f0r_get_param_value should return the same values after a call
 * to \ref f0r_update as before the call).
 *
 * The function is responsible to restore the fpu state (e.g. rounding mode)
 * and mmx state if applicable before it returns to the caller.
 *
 * The host mustn't call \ref f0r_update for effects of type
 * \ref F0R_PLUGIN_TYPE_MIXER2 and \ref F0R_PLUGIN_TYPE_MIXER3.
 *
 * \param instance the effect instance
 * \param time the application time in seconds but with subsecond resolution
 *        (e.g. milli-second resolution). The resolution should be at least
 *        the inter-frame period of the application.
 * \param inframe the incoming video frame (can be zero for sources)
 * \param outframe the resulting video frame
 *
 * \see f0r_update2
 */
void f0r_update(f0r_instance_t instance, 
		double time, const guint32* inframe, guint32* outframe);

//---------------------------------------------------------------------------

/**
 * For effects of type \ref F0R_PLUGIN_TYPE_SOURCE or
 * \ref F0R_PLUGIN_TYPE_FILTER this method is optional. The \ref f0r_update
 * method must still be exported for these two effect types. If both are
 * provided the behavior of them must be the same.
 *
 * Effects of type \ref F0R_PLUGIN_TYPE_MIXER2 or \ref F0R_PLUGIN_TYPE_MIXER3 must provide the new \ref f0r_update2 method.

 * \param instance the effect instance
 * \param time the application time in seconds but with subsecond resolution
 *        (e.g. milli-second resolution). The resolution should be at least
 *        the inter-frame period of the application.
 * \param inframe1 the first incoming video frame (can be zero for sources)
 * \param inframe2 the second incoming video frame
          (can be zero for sources and filters)
 * \param inframe3 the third incoming video frame
          (can be zero for sources, filters and mixer2) 
 * \param outframe the resulting video frame
 *
 * \see f0r_update
 */
void f0r_update2(f0r_instance_t instance,
		 double time,
		 const guint32* inframe1,
		 const guint32* inframe2,
		 const guint32* inframe3,
		 guint32* outframe);
//---------------------------------------------------------------------------

#endif
