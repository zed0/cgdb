/* wm.h:
 * -----
 */

#ifndef __CGDB_WM_H__
#define __CGDB_WM_H__

#include "window.h"

/**
 * @file
 * wm.h
 *
 * @brief
 * This is the interface to the window management library.
 */

/**
 * Direction values.
 */
typedef enum {
    HORIZONTAL,
    VERTICAL,
    BOTH
} wm_direction;

/**
 * @name Window Management Overview
 * Libwm is the window manager library for CGDB.  This abstracts the
 * management of "windows" in the terminal, so that the calling program can
 * create, arrange and delete windows in the terminal space.  This management
 * is transparent to the confined window, which can do input and drawing
 * without knowledge of actual screen coordinates or whether it is seen by the
 * user at all.
 */

/**
 * The window manager object, one is required for any window management
 * operations.
 */
struct window_manager_s;
typedef struct window_manager_s window_manager;

/**
 * Creates a new window manager.  When done with this object, call wm_destroy
 * to free it.
 *
 * @param main_window
 * The initial window, which will be the top level window until any kind of
 * splitting occurs.
 *
 * @return
 * A new window manager is returned, or NULL on error.
 */
window_manager *wm_create(wm_window *main_window);

/**
 * Free the window manager, recursively destroying all associated windows.
 *
 * @param wm
 * The window manager to destroy.
 *
 * @return
 * Zero on success, non-zero on failure.
 */
int wm_destroy(window_manager *wm);

/**
 * Redraw all visible windows (because the display was damaged for some reason,
 * or maybe the user hit C-l).
 *
 * @param wm
 * The window manager.
 *
 * @return
 * Zero on success, non-zero on failure.
 */
int wm_redraw(window_manager *wm);

/**
 * Split the current window, creating a new window which will divide the space
 * occupied by the original window.
 *
 * @param window
 * The window object to place in the newly created space.
 *
 * @param orientation
 * Orientation of the split (HORIZONTAL or VERTICAL).
 *
 * @return
 * Zero on success, non-zero on failure.
 */
int wm_split(wm_window *window, wm_direction orientation);

/**
 * Close the current window.  Remaining windows will be shuffled to fill in
 * empty screen real estate.
 *
 * @param window
 * The window to close.
 *
 * @return
 * Zero on success, non-zero on failure.
 */
void wm_close_current(wm_window *window);

/**
 * @name WM Options
 * The data types and operations used to set and retrieve options for the
 * window manager.  The options are used to control the behavior of the
 * windows manager in a way that emulates Vim's window system.
 */

/**
 * Set of options that affect window manager behavior.
 */
typedef enum {

    /**
     * Option "cmdheight" (shorthand: "ch") is of type: integer
     */
    CMDHEIGHT,

    /**
     * Option "eadirection" (shorthand: "ead") is of type: wm_direction
     */
    EADIRECTION,

    /**
     * Option "equalalways" (shorthand: "ea") is of type: boolean
     */
    EQUALALWAYS,

    /**
     * Option "splitbelow" (shorthand: "sb") is of type: boolean
     */
    SPLITBELOW,

    /**
     * Option "splitright" (shorthand: "spr") is of type: boolean
     */
    SPLITRIGHT,

    /**
     * Option "winfixheight" (shorthand: "wfh") is of type: boolean
     */
    WINFIXHEIGHT,

    /**
     * Option "winminheight" (shorthand: "wmh") is of type: integer
     */
    WINMINHEIGHT,

    /**
     * Option "winminwidth" (shorthand: "wmw") is of type: integer
     */
    WINMINWIDTH,

    /**
     * Option "winheight" (shorthand: "wh") is of type: integer
     */
    WINHEIGHT,

    /**
     * Option "winwidth" (shorthand: "wiw") is of type: integer
     */
    WINWIDTH
} wm_option;

/**
 * Option value types
 */
typedef enum {
    WM_INTEGER,
    WM_BOOLEAN,
    WM_EADIR,
    WM_UNKNOWN
} wm_opttype;

/**
 * An option setting
 */
typedef struct {

    /**
     * Type of option this structure describes
     */
    wm_opttype type;

    /**
     * Actual value of option is stored in this variant type
     */
    union {
        /** if (type == WM_INTEGER) */
        int int_val;

        /** if (type == WM_BOOLEAN) */
        char bool_val;

        /** if (type == WM_EADIR) */
        wm_direction ead_val;
    } variant;

} wm_optval;

/**
 * Get the value of the specified option.
 *
 * @param   option  The option value to retrieve.
 *
 * @return  The value of the specified option.  The type will be set
 *          to WM_UNKNOWN if an unknown option is specified, in which case
 *          the variant is undefined.
 */
wm_optval wm_option_get(wm_option option);

/**
 * Set the value of the specified option.
 *
 * @param   option  The option to set.
 * @param   value   The value to set for the given option.
 *
 * @return  Zero on success, non-zero on failure.
 */
int wm_option_set(wm_option option, wm_optval value);

#endif /* __CGDB_WM_H__ */
