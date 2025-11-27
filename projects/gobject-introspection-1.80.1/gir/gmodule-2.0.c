/************************************************************/
/* THIS FILE IS GENERATED DO NOT EDIT */
/************************************************************/

/**
 * GModule:
 *
 * The #GModule struct is an opaque data structure to represent a
 * [dynamically-loaded module][glib-Dynamic-Loading-of-Modules].
 * It should only be accessed via the following functions.
 */


/**
 * GModuleCheckInit:
 * @module: the #GModule corresponding to the module which has just been loaded
 *
 * Specifies the type of the module initialization function.
 * If a module contains a function named g_module_check_init() it is called
 * automatically when the module is loaded. It is passed the #GModule structure
 * and should return %NULL on success or a string describing the initialization
 * error.
 *
 * Returns: %NULL on success, or a string describing the initialization error
 */


/**
 * GModuleUnload:
 * @module: the #GModule about to be unloaded
 *
 * Specifies the type of the module function called when it is unloaded.
 * If a module contains a function named g_module_unload() it is called
 * automatically when the module is unloaded.
 * It is passed the #GModule structure.
 */


/**
 * G_MODULE_ERROR:
 *
 * The error domain of the #GModule API.
 *
 * Since: 2.70
 */


/**
 * G_MODULE_EXPORT:
 *
 * Used to declare functions exported by libraries or modules.
 *
 * When compiling for Windows, it marks the symbol as `dllexport`.
 *
 * When compiling for Linux and Unices, it marks the symbol as having `default`
 * visibility. This is no-op unless the code is being compiled with a
 * non-default
 * [visibility flag](https://gcc.gnu.org/onlinedocs/gcc/Code-Gen-Options.html#index-fvisibility-1260)
 * such as `hidden`.
 *
 * This macro must only be used when compiling a shared module. Modules that
 * support both shared and static linking should define their own macro that
 * expands to %G_MODULE_EXPORT when compiling the shared module, but is empty
 * when compiling the static module on Windows.
 */


/**
 * G_MODULE_IMPORT:
 *
 * Used to declare functions imported from modules.
 */


/**
 * G_MODULE_SUFFIX:
 *
 * Expands to a shared library suffix for the current platform without the
 * leading dot. On Unixes this is "so", and on Windows this is "dll".
 *
 * Deprecated: 2.76: Use g_module_open() instead with @module_name as the
 * basename of the file_name argument. You will get the wrong results using
 * this macro most of the time:
 *
 * 1. The suffix on macOS is usually 'dylib', but it's 'so' when using
 *    Autotools, so there's no way to get the suffix correct using
 *    a pre-processor macro.
 * 2. Prefixes also vary in a platform-specific way. You may or may not have
 *    a 'lib' prefix for the name on Windows and on Cygwin the prefix is
 *    'cyg'.
 * 3. The library name itself can vary per platform. For instance, you may
 *    want to load foo-1.dll on Windows and libfoo.1.dylib on macOS.
 *
 * g_module_open() takes care of all this by searching the filesystem for
 * combinations of possible suffixes and prefixes.
 */


/**
 * g_module_build_path:
 * @directory: (nullable): the directory where the module is. This can be
 *     %NULL or the empty string to indicate that the standard platform-specific
 *     directories will be used, though that is not recommended
 * @module_name: the name of the module
 *
 * A portable way to build the filename of a module. The platform-specific
 * prefix and suffix are added to the filename, if needed, and the result
 * is added to the directory, using the correct separator character.
 *
 * The directory should specify the directory where the module can be found.
 * It can be %NULL or an empty string to indicate that the module is in a
 * standard platform-specific directory, though this is not recommended
 * since the wrong module may be found.
 *
 * For example, calling g_module_build_path() on a Linux system with a
 * @directory of `/lib` and a @module_name of "mylibrary" will return
 * `/lib/libmylibrary.so`. On a Windows system, using `\Windows` as the
 * directory it will return `\Windows\mylibrary.dll`.
 *
 * Returns: the complete path of the module, including the standard library
 *     prefix and suffix. This should be freed when no longer needed
 * Deprecated: 2.76: Use g_module_open() instead with @module_name as the
 * basename of the file_name argument. See %G_MODULE_SUFFIX for why.
 */


/**
 * g_module_close:
 * @module: a #GModule to close
 *
 * Closes a module.
 *
 * Returns: %TRUE on success
 */


/**
 * g_module_error:
 *
 * Gets a string describing the last module error.
 *
 * Returns: a string describing the last module error
 */


/**
 * g_module_make_resident:
 * @module: a #GModule to make permanently resident
 *
 * Ensures that a module will never be unloaded.
 * Any future g_module_close() calls on the module will be ignored.
 */


/**
 * g_module_name:
 * @module: a #GModule
 *
 * Returns the filename that the module was opened with.
 *
 * If @module refers to the application itself, "main" is returned.
 *
 * Returns: (transfer none): the filename of the module
 */


/**
 * g_module_open:
 * @file_name: (nullable): the name or path to the file containing the module,
 *     or %NULL to obtain a #GModule representing the main program itself
 * @flags: the flags used for opening the module. This can be the
 *     logical OR of any of the #GModuleFlags.
 *
 * A thin wrapper function around g_module_open_full()
 *
 * Returns: a #GModule on success, or %NULL on failure
 */


/**
 * g_module_open_full:
 * @file_name: (nullable): the name or path to the file containing the module,
 *     or %NULL to obtain a #GModule representing the main program itself
 * @flags: the flags used for opening the module. This can be the
 *     logical OR of any of the #GModuleFlags
 * @error: #GError.
 *
 * Opens a module. If the module has already been opened, its reference count
 * is incremented. If not, the module is searched in the following order:
 *
 * 1. If @file_name exists as a regular file, it is used as-is; else
 * 2. If @file_name doesn't have the correct suffix and/or prefix for the
 *    platform, then possible suffixes and prefixes will be added to the
 *    basename till a file is found and whatever is found will be used; else
 * 3. If @file_name doesn't have the ".la"-suffix, ".la" is appended. Either
 *    way, if a matching .la file exists (and is a libtool archive) the
 *    libtool archive is parsed to find the actual file name, and that is
 *    used.
 *
 * At the end of all this, we would have a file path that we can access on
 * disk, and it is opened as a module. If not, @file_name is opened as
 * a module verbatim in the hopes that the system implementation will somehow
 * be able to access it.
 *
 * Returns: a #GModule on success, or %NULL on failure
 * Since: 2.70
 */


/**
 * g_module_supported:
 *
 * Checks if modules are supported on the current platform.
 *
 * Returns: %TRUE if modules are supported
 */


/**
 * g_module_symbol:
 * @module: a #GModule
 * @symbol_name: the name of the symbol to find
 * @symbol: (out): returns the pointer to the symbol value
 *
 * Gets a symbol pointer from a module, such as one exported
 * by %G_MODULE_EXPORT. Note that a valid symbol can be %NULL.
 *
 * Returns: %TRUE on success
 */



/************************************************************/
/* THIS FILE IS GENERATED DO NOT EDIT */
/************************************************************/
