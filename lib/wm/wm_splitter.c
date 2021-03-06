#include <assert.h>
#include <stdlib.h>

#include "wm.h"
#include "wm_splitter.h"

/* Forward declarations */
static int wm_splitter_init(wm_window *window);
static int wm_splitter_destroy(wm_window *window);
static int wm_splitter_redraw(wm_window *window);
static int wm_splitter_layout(wm_window *window);
static void wm_splitter_minimum_size(wm_window *window,
                                     int *height, int *width);
static int wm_splitter_find_child(wm_splitter *splitter, wm_window *window);
static int wm_splitter_array_remove(wm_splitter *splitter, wm_window *window);
static int wm_splitter_place_window(wm_window *window, int top, int left,
                                    int height, int width);
static int wm_splitter_min_dimension(wm_splitter *splitter, wm_window *window);
static wm_window *wm_splitter_find_window_at(wm_splitter *splitter,
                                             wm_position cursor_pos);

/* Initial size of the splitter's "children" array. */
const int DEFAULT_ARRAY_LENGTH = 4;

/* Quick macro to fetch a window position or dimension along the split */
#define get_position(splitter, window) \
    (*((splitter)->orientation == WM_HORIZONTAL ? &((window)->top) \
                                              : &((window)->left)))
#define get_dimension(splitter, window) \
    (*((splitter)->orientation == WM_HORIZONTAL ? &((window)->real_height) \
                                              : &((window)->real_width)))

wm_splitter *
wm_splitter_create(wm_orientation orientation)
{
    wm_splitter *splitter = (wm_splitter *) malloc(sizeof(wm_splitter));
    wm_window_init((wm_window *) splitter);

    /* Window setup */
    splitter->window.destroy = wm_splitter_destroy;
    splitter->window.layout = wm_splitter_layout;
    splitter->window.redraw = wm_splitter_redraw;
    splitter->window.minimum_size = wm_splitter_minimum_size;
    splitter->window.is_splitter = 1;
    wm_window_show_status_bar((wm_window *) splitter, 0);

    /* Splitter data */
    splitter->orientation = orientation;
    splitter->children = malloc(sizeof(wm_window *) * DEFAULT_ARRAY_LENGTH);
    splitter->num_children = 0;
    splitter->array_length = DEFAULT_ARRAY_LENGTH;

    return splitter;
}

int
wm_splitter_remove(wm_splitter *splitter, wm_window *window)
{
    int need_focus = wm_is_focused(window->wm, window);

    /* This should be essentially invariant */
    assert(splitter->num_children >= 2);

    if (wm_splitter_array_remove(splitter, window)) {
        return -1;
    }
    wm_window_destroy(window);

    if (splitter->num_children == 1) {
        wm_window *self = (wm_window *) splitter;
        wm_splitter *parent = (wm_splitter *) self->parent;
        wm_window *child = splitter->children[0];
        wm_splitter_array_remove(splitter, child);
        if (parent) {
            int i = 0;
            for (i = 0; i < parent->num_children; ++i) {
                if (parent->children[i] == self) {
                    parent->children[i] = child;
                    break;
                }
            }
            wm_window_set_context(child, parent->window.wm, self->parent,
                                  self->cwindow);
            if (need_focus) {
                wm_focus(child->wm, child);
            }
            self->cwindow = NULL; /* We gave our cwindow to child */
            wm_window_destroy(self);
            wm_window_layout_event((wm_window *) parent);
        } else {
            wm_new_main(self->wm, child);
        }
    } else {
        if (need_focus) {
            wm_focus(splitter->window.wm, splitter->children[0]);
        }
        wm_window_layout_event((wm_window *) splitter);
    }

    return 0;
}

int wm_splitter_resize_window(wm_splitter *splitter, wm_window *window,
                              wm_orientation dir, int size)
{
    int i;
    int desired_change;
    int actual_change = 0;
    int min = wm_splitter_min_dimension(splitter, window);
    int max;

    /* See TODO in wm_splitter_remove(). */
    if (splitter->num_children == 1) {
        return -1;
    }
    if (dir != splitter->orientation) {
        /* Handled in wm_resize(). */
        return -1;
    }

    /* Limit to what is possible */
    if (size < min) {
        size = min;
    }
    if (dir == WM_HORIZONTAL) {
        max = splitter->window.real_height;
    } else {
        /* Leave space for vertical separators between children */
        max = splitter->window.real_width - (splitter->num_children-1);
    }
    for (i = 0; i < splitter->num_children; ++i) {
        wm_window *child = splitter->children[i];
        if (child != window) {
            max -= wm_splitter_min_dimension(splitter, child);
        }
    }
    if (size > max) {
        size = max;
    }
    desired_change = size - get_dimension(splitter, window);
    if (desired_change == 0) {
        return 0;
    }

    /* Find window we're dealing with */
    i = wm_splitter_find_child(splitter, window);
    if (i < 0) {
        return -1;
    }

    if (desired_change < 0) {
        wm_window *next = NULL;
        int wrapped = 0;
        if (i+1 == splitter->num_children) {
            next = splitter->children[i-1];
            wrapped = 1;
        } else {
            next = splitter->children[i+1];
        }
        get_dimension(splitter, next) -= desired_change;
        get_dimension(splitter, window) += desired_change;
        if (wrapped) {
            get_position(splitter, window) -= desired_change;
        } else {
            get_position(splitter, next) += desired_change;
        }
    } else if (desired_change > 0) {
        /* Borrow from successors */
        int j = i + 1, k;
        while (actual_change != desired_change && j < splitter->num_children) {
            int avail = get_dimension(splitter, splitter->children[j]) -
                wm_splitter_min_dimension(splitter, splitter->children[j]);
            int this_change = desired_change - actual_change;
            if (this_change > avail) {
                this_change = avail;
            }
            actual_change += this_change;
            get_position(splitter, splitter->children[j]) += this_change;
            get_dimension(splitter, splitter->children[j]) -= this_change;
            get_dimension(splitter, window) += this_change;
            for (k = i + 1; k < j; ++k) {
                get_position(splitter, splitter->children[k]) += this_change;
            }
            ++j;
        }
        /* Borrow from predecessors */
        j = i - 1;
        while (actual_change != desired_change && j >= 0) {
            int avail = get_dimension(splitter, splitter->children[j]) -
                wm_splitter_min_dimension(splitter, splitter->children[j]);
            int this_change = desired_change - actual_change;
            if (this_change > avail) {
                this_change = avail;
            }
            actual_change += this_change;
            get_dimension(splitter, splitter->children[j]) -= this_change;
            get_position(splitter, window) -= this_change;
            get_dimension(splitter, window) += this_change;
            for (k = i - 1; k > j; --k) {
                get_position(splitter, splitter->children[k]) -= this_change;
            }
            --j;
        }
    }

    /* Place windows that have been updated */
    for (i = 0; i < splitter->num_children; ++i) {
        int cheight, cwidth;
        int ctop, cleft;
        wm_window *child = splitter->children[i];
        getbegyx(child->cwindow, ctop, cleft);
        getmaxyx(child->cwindow, cheight, cwidth);
        if (child->top != ctop || child->left != cleft ||
            child->real_height != cheight || child->real_width != cwidth)
        {
            wm_splitter_place_window(child, child->top, child->left,
                                     child->real_height, child->real_width);
        }
    }

    return 0;
}

int
wm_splitter_split(wm_splitter *splitter, wm_window *window,
                  wm_window *new_window, wm_orientation orientation)
{
    wm_window *obj = new_window;
    int pos = splitter->num_children;
    int i;

    /* Find first window for splitting/insertion */
    if (window) {
        i = wm_splitter_find_child(splitter, window);
        if (i < 0) {
            return -1;
        }
        pos = i + 1;
    } else if (orientation != splitter->orientation) {
        /* Tried to append a window and got the orientation wrong. */
        return -1;
    }

    /* TODO: Handle splitright, splitbelow options. */
    if (orientation == splitter->orientation) {
        WINDOW *cwindow = derwin(splitter->window.cwindow, 1, 1, 0, 0);
        if (cwindow == NULL) {
            abort();
        }
        wm_window_set_context(new_window, splitter->window.wm,
                              (wm_window *) splitter, cwindow);
    } else {
        wm_splitter *new_splitter = wm_splitter_create(orientation);
        wm_window_set_context((wm_window *) new_splitter, splitter->window.wm,
                              (wm_window *) splitter, window->cwindow);
        wm_splitter_array_remove(splitter, window);
        wm_splitter_split(new_splitter, NULL, window, orientation);
        wm_splitter_split(new_splitter, window, new_window, orientation);
        obj = (wm_window *) new_splitter;
        pos--; /* Since we're replacing window instead of appending after it */
    }

    if (splitter->num_children == splitter->array_length) {
        splitter->array_length *= 2;
        splitter->children = realloc(splitter->children,
            sizeof(wm_window *) * splitter->array_length);
    }
    for (i = pos; i < splitter->num_children; ++i) {
        splitter->children[i+1] = splitter->children[i];
    }
    splitter->children[pos] = obj;
    splitter->num_children++;

    return wm_window_layout_event((wm_window *) splitter);
}

wm_window *wm_splitter_get_neighbor(wm_splitter *splitter, wm_window *window,
                                    wm_direction dir, wm_position cursor_pos)
{
    wm_splitter *parent = NULL;
    wm_window *result = NULL;
    int i;

    if (splitter->window.parent) {
        assert(splitter->window.parent->is_splitter);
        parent = (wm_splitter *) splitter->window.parent;
    }
    if (splitter->orientation == WM_HORIZONTAL &&
            (dir == WM_LEFT || dir == WM_RIGHT) ||
        splitter->orientation == WM_VERTICAL &&
            (dir == WM_UP || dir == WM_DOWN))
    {
        if (parent == NULL) {
            return NULL;
        }
        return wm_splitter_get_neighbor(parent, (wm_window *) splitter,
                                        dir, cursor_pos);
    }

    switch (dir) {
        case WM_UP:
        case WM_LEFT:
            for (i = 1; i < splitter->num_children; ++i) {
                if (splitter->children[i] == window) {
                    result = splitter->children[i-1];
                    break;
                }
            }
            break;

        case WM_DOWN:
        case WM_RIGHT:
            for (i = 0; i < splitter->num_children - 1; ++i) {
                if (splitter->children[i] == window) {
                    result = splitter->children[i+1];
                    break;
                }
            }
            break;
    }

    if (result && result->is_splitter) {
        result = wm_splitter_find_window_at((wm_splitter *) result, cursor_pos);
    }

    return result;
}

/* Window method implementations */

static int
wm_splitter_destroy(wm_window *window)
{
    wm_splitter *splitter = (wm_splitter *) window;
    int i;

    for (i = 0; i < splitter->num_children; ++i) {
        wm_window_destroy(splitter->children[i]);
    }
    free(splitter->children);

    return 0;
}

static int
wm_splitter_layout(wm_window *window)
{
    /* TODO: Handle window size options (min height, etc?) */
    wm_splitter *splitter = (wm_splitter *) window;
    int sum = 0, remainder = 0;
    int position;
    float *proportions = malloc(sizeof(float) * splitter->num_children);
    int prev_dimension = 0;
    int *new_sizes = malloc(sizeof(int) * splitter->num_children);
    int redistribute = 0; /* Set true as fallback if things look bad */
    int i;
    int real_width = window->real_width - (splitter->num_children - 1);

    for (i = 0; i < splitter->num_children; ++i) {
        wm_window *child = splitter->children[i];
        int min = wm_splitter_min_dimension(splitter, child);
        int cur = get_dimension(splitter, child);
        prev_dimension += cur;
        proportions[i] = cur;
        if (splitter->orientation == WM_HORIZONTAL) {
            if (window->real_height * proportions[i] < min) {
                redistribute = 1;
                break;
            }
        } else {
            if (real_width * proportions[i] < min) {
                redistribute = 1;
                break;
            }
        }
        /* Note: Special case: initially created windows are 1 x 1. */
        if (child->real_height == 1 && child->real_width == 1) {
            redistribute = 1;
            break;
        }
    }
    if (redistribute) {
        /* Distribute windows equally */
        if (splitter->orientation == WM_HORIZONTAL) {
            for (i = 0; i < splitter->num_children; i++) {
                new_sizes[i] = window->real_height / splitter->num_children;
            }
            sum = window->real_height;
        } else {
            for (i = 0; i < splitter->num_children; i++) {
                new_sizes[i] = real_width / splitter->num_children;
            }
            sum = real_width;
        }
    } else {
        /* Distribute windows according to previous proportions. */
        sum = 0;
        for (i = 0; i < splitter->num_children; ++i) {
            proportions[i] /= prev_dimension;
            if (splitter->orientation == WM_HORIZONTAL) {
                new_sizes[i] = proportions[i] * window->real_height;
            } else {
                new_sizes[i] = proportions[i] * real_width;
            }
            sum += new_sizes[i];
        }
    }

    if (splitter->orientation == WM_HORIZONTAL) {
        remainder = window->real_height - sum;
        position = window->top;
    } else {
        remainder = real_width - sum;
        position = window->left;
    }

    for (i = 0; i < splitter->num_children; ++i) {
        int my_dimension = new_sizes[i];
        wm_window *child = splitter->children[i];
        while (my_dimension < wm_splitter_min_dimension(splitter, child)
               && remainder) {
            my_dimension++;
            remainder--;
        }
        if (remainder && i == splitter->num_children - 1) {
            my_dimension += remainder;
            remainder = 0;
        }
        /* Resize and relocate the window */
        if (splitter->orientation == WM_HORIZONTAL) {
            wm_splitter_place_window(child, position, window->left,
                                     my_dimension, window->real_width);
            position += my_dimension;
        } else {
            wm_splitter_place_window(child, window->top, position,
                                     window->real_height, my_dimension);
            position += my_dimension + 1;
        }
        /* Notify */
        wm_window_layout_event(child);
    }

    free(new_sizes);
    free(proportions);
    return wm_splitter_redraw(window);
}

static int
wm_splitter_redraw(wm_window *window)
{
    wm_splitter *splitter = (wm_splitter *) window;
    int i, j;
    int num_children = splitter->num_children;

    /* Clear the window - mainly useful for testing purposes, to make
     * rendering issues obvious. */
    werase(window->cwindow);
    /* Lay down an empty "status bar" beneath vertical splits, because the
     * children cannot draw beneath the vsplit itself (it's outside of the
     * child window.) The child will overwrite the rest of this. */
    if (splitter->orientation == WM_VERTICAL) {
        wattron(window->cwindow, WA_REVERSE);
        for (i = 0; i < window->real_width; ++i) {
            mvwprintw(window->cwindow, window->real_height-1, i, " ");
        }
        wattroff(window->cwindow, WA_REVERSE);
    }
    wrefresh(window->cwindow);
    /* Render the child windows */
    for (i = 0; i < num_children; ++i) {
        wm_window_redraw(splitter->children[i]);
        /* Draw a vertical separator for vsplits */
        if (splitter->orientation == WM_VERTICAL && i < num_children-1) {
            int left = splitter->children[i]->left +
                       splitter->children[i]->real_width;
            /* TODO: Configurable color of separator */
            wattron(window->cwindow, WA_REVERSE);
            for (j = 0; j < window->height-1; ++j) {
                mvwprintw(window->cwindow, j, left, "|");
            }
            wattroff(window->cwindow, WA_REVERSE);
            /* Not sure why this is needed, curses sucks */
            wrefresh(window->cwindow);
        }
    }
    wrefresh(window->cwindow);

    return 0;
}

static void
wm_splitter_minimum_size(wm_window *window, int *height, int *width)
{
    wm_splitter *splitter = (wm_splitter *) window;
    *height = *width = 0;
    int i;

    for (i = 0; i < splitter->num_children; ++i) {
        wm_window *child = splitter->children[i];
        int child_height, child_width;
        child->minimum_size(child, &child_height, &child_width);
        if (splitter->orientation == WM_HORIZONTAL) {
            *height += child_height;
            if (child_width > *width) {
                *width = child_width;
            }
        } else {
            *width += child_width;
            if (child_height > *height) {
                *height = child_height;
            }
        }
    }
}

static int
wm_splitter_find_child(wm_splitter *splitter, wm_window *window)
{
    int i = 0;
    for (i = 0; i < splitter->num_children; ++i) {
        if (splitter->children[i] == window) {
            return i;
        }
    }
    return -1;
}

static int
wm_splitter_array_remove(wm_splitter *splitter, wm_window *window)
{
    int i = wm_splitter_find_child(splitter, window);
    int j;
    if (i < 0) {
        return -1;
    }

    splitter->num_children--;
    for (j = i; j < splitter->num_children; ++j) {
        splitter->children[j] = splitter->children[j+1];
    }

    return 0;
}

/* Helper function: Get minimum dimension of window along split. */
static int
wm_splitter_min_dimension(wm_splitter *splitter, wm_window *window)
{
    int min_height, min_width;

    window->minimum_size(window, &min_height, &min_width);
    return splitter->orientation == WM_HORIZONTAL ? min_height : min_width;
}

static int
wm_splitter_place_window(wm_window *window, int top, int left,
                         int height, int width)
{
    /* Note: Assumes resizes are always smaller or bigger, not (smaller height
     * larger width). Curses window operations will fail if the window tries
     * to grow or move into a space that is out of bounds, so order is
     * important. */
    wresize(window->cwindow, height, width);
    mvwin(window->cwindow, top, left);
    wresize(window->cwindow, height, width);
    mvwin(window->cwindow, top, left);

    wm_window_layout_event(window);
}

static wm_window *
wm_splitter_find_window_at(wm_splitter *splitter, wm_position cursor_pos)
{
    wm_window *result = NULL;
    int i;

    for (i = 0; i < splitter->num_children; ++i) {
        wm_window *child = splitter->children[i];
        int value, lower_bound, upper_bound;
        if (splitter->orientation == WM_HORIZONTAL) {
            value = cursor_pos.top;
            lower_bound = child->top;
            upper_bound = child->top + child->real_height;
        } else {
            value = cursor_pos.left;
            lower_bound = child->left;
            upper_bound = child->left + child->real_width;
        }
        if (value >= lower_bound && value < upper_bound ||
            i == 0 && value < lower_bound ||
            i == splitter->num_children - 1 && value >= upper_bound)
        {
            result = child;
            break;
        }
    }
    if (result && result->is_splitter) {
        result = wm_splitter_find_window_at((wm_splitter *) result, cursor_pos);
    }

    return result;
}
