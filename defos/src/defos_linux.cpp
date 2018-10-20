#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_LINUX)

/*
    some resources to manage window for x11.

    1. https://tronche.com/gui/x/xlib/
    2. https://github.com/glfw/glfw/blob/master/src/x11_window.c
    3. https://github.com/yetanothergeek/xctrl/blob/master/src/xctrl.c
*/

#include "defos_private.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/cursorfont.h>
#include <Xcursor.h>
#include <Xrandr.h>
#include <stdlib.h>

//static GC gc;
#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
#define _NET_WM_STATE_TOGGLE 2
#define XATOM(name) XInternAtom(disp, name, False)

static Display *disp;
static int screen;
static Window win;
static Window root;

// TODO: add support checking
static Atom UTF8_STRING;
static Atom NET_WM_NAME;
static Atom NET_WM_STATE;
static Atom NET_WM_STATE_FULLSCREEN;
static Atom NET_WM_STATE_MAXIMIZED_VERT;
static Atom NET_WM_STATE_MAXIMIZED_HORZ;
static Atom NET_WM_ACTION_MINIMIZE;

// TODO: should query state from system
static bool is_maximized = false;
static bool is_fullscreen = false;

static Cursor custom_cursor; // image cursor
static bool has_custom_cursor = false;

static bool is_window_visible(Window window);
static void send_message(Window &window, Atom type, long a, long b, long c, long d, long e);

void defos_init()
{
    disp = XOpenDisplay(NULL);
    screen = DefaultScreen(disp);
    win = dmGraphics::GetNativeX11Window();
    root = XDefaultRootWindow(disp);

    // from https://specifications.freedesktop.org/wm-spec/1.3/ar01s05.html
    UTF8_STRING = XATOM("UTF8_STRING");
    NET_WM_NAME = XATOM("_NET_WM_NAME");
    NET_WM_STATE = XATOM("_NET_WM_STATE");
    NET_WM_STATE_FULLSCREEN = XATOM("_NET_WM_STATE_FULLSCREEN");
    NET_WM_STATE_MAXIMIZED_VERT = XATOM("_NET_WM_STATE_MAXIMIZED_VERT");
    NET_WM_STATE_MAXIMIZED_HORZ = XATOM("_NET_WM_STATE_MAXIMIZED_HORZ");
}

void defos_final()
{
    if (has_custom_cursor)
    {
        XFreeCursor(disp, custom_cursor);
        has_custom_cursor = false;
    }
}

void defos_event_handler_was_set(DefosEvent event)
{
}

bool defos_is_fullscreen()
{
    return is_fullscreen;
}

bool defos_is_maximized()
{
    return is_maximized;
}

bool defos_is_mouse_in_view()
{
    return false;
}

void defos_disable_maximize_button()
{
    dmLogInfo("Method 'defos_disable_maximize_button' is not supported in Linux");
}

void defos_disable_minimize_button()
{
    dmLogInfo("Method 'defos_disable_minimize_button' is not supported in Linux");
}

void defos_disable_window_resize()
{
    dmLogInfo("Method 'defos_disable_window_resize' is not supported in Linux");
}

void defos_set_cursor_visible(bool visible)
{
    dmLogInfo("Method 'defos_set_cursor_visible' is not supported in Linux");
}

bool defos_is_cursor_visible()
{
    return false;
}

void defos_toggle_fullscreen()
{
    if (!is_fullscreen)
    {
        send_message(win,
                     NET_WM_STATE,
                     _NET_WM_STATE_ADD,
                     NET_WM_STATE_FULLSCREEN,
                     0,
                     1,
                     0);
        ;
    }
    else
    {
        send_message(win,
                     NET_WM_STATE,
                     _NET_WM_STATE_REMOVE,
                     NET_WM_STATE_FULLSCREEN,
                     0,
                     1,
                     0);
    }

    is_fullscreen = !is_fullscreen;
    XFlush(disp);
}

void defos_toggle_maximized()
{
    if (!is_maximized)
    {
        send_message(win,
                     NET_WM_STATE,
                     _NET_WM_STATE_ADD,
                     NET_WM_STATE_MAXIMIZED_VERT,
                     NET_WM_STATE_MAXIMIZED_HORZ,
                     1,
                     0);
    }
    else
    {
        send_message(win,
                     NET_WM_STATE,
                     _NET_WM_STATE_REMOVE,
                     NET_WM_STATE_MAXIMIZED_VERT,
                     NET_WM_STATE_MAXIMIZED_HORZ,
                     1,
                     0);
    }

    is_maximized = !is_maximized;
    XFlush(disp);
}

void defos_set_console_visible(bool visible)
{
    dmLogInfo("Method 'defos_set_console_visible' is not supported in Linux");
}

bool defos_is_console_visible()
{
    return false;
}

void defos_set_window_size(float x, float y, float w, float h)
{
    // change size only if it is visible
    if (is_window_visible(win))
    {
        if (isnan(x) || isnan(y))
        {
            XWindowAttributes attributes;
            XGetWindowAttributes(disp, root, &attributes);

            x = ((float)attributes.width - w) / 2;
            y = ((float)attributes.height - h) / 2;
        }

        XMoveResizeWindow(disp, win, (int)x, (int)y, (unsigned int)w, (unsigned int)h);
        XFlush(disp);
    }
}

void defos_set_view_size(float x, float y, float w, float h)
{
    XWindowChanges changes;
    changes.x = (int)x;
    changes.y = (int)y;
    changes.width = (int)w;
    changes.height = (int)h;

    XConfigureWindow(disp, win, CWX | CWY | CWWidth | CWHeight, &changes);
    XFlush(disp);
}

void defos_set_window_title(const char *title_lua)
{
    XChangeProperty(disp, win, NET_WM_NAME, UTF8_STRING, 8, PropModeReplace, (unsigned char *)title_lua, strlen(title_lua));
    XFlush(disp); // IMPORTANT: we have to flush, or nothing will be changed
}

WinRect defos_get_window_size()
{
    WinRect r = {0.0f, 0.0f, 0.0f, 0.0f};
    return r;
}

WinRect defos_get_view_size()
{
    int x, y;
    unsigned int w, h, bw, depth;

    Window dummy;
    XGetGeometry(disp, win, &dummy, &x, &y, &w, &h, &bw, &depth);
    XTranslateCoordinates(disp, win, root, x, y, &x, &y, &dummy);

    WinRect r = {(float)x, (float)y, (float)w, (float)h};
    return r;
}

void defos_set_cursor_pos(float x, float y)
{
    XWarpPointer(disp, None, root, 0, 0, 0, 0, (int)x, (int)y);
    XFlush(disp);
}

void defos_move_cursor_to(float x, float y)
{
    WinRect rect = defos_get_window_size();

    int ix = (int)x;
    int iy = (int)y;

    // TODO: need this?
    if (ix > rect.w)
        ix = rect.w;
    if (ix < 0)
        ix = 0;
    if (iy > rect.h)
        iy = rect.h;
    if (iy < 0)
        iy = 0;

    XWarpPointer(disp, None, win, 0, 0, 0, 0, ix, iy);
    XFlush(disp);
}

void defos_set_cursor_clipped(bool clipped)
{
    dmLogInfo("Method 'defos_set_cursor_clipped' is not supported in Linux");
}

bool defos_is_cursor_clipped()
{
    return false;
}

void defos_set_cursor_locked(bool locked)
{
    dmLogInfo("Method 'defos_set_cursor_locked' is not supported in Linux");
}

bool defos_is_cursor_locked()
{
    return false;
}

void defos_update()
{
}

void defos_set_custom_cursor_linux(const char *filename)
{
    Cursor cursor = XcursorFilenameLoadCursor(disp, filename);
    XDefineCursor(disp, win, cursor);
    if (has_custom_cursor) { XFreeCursor(disp, custom_cursor); }
    custom_cursor = cursor;
    has_custom_cursor = true;
}

static unsigned int get_cursor(DefosCursor cursor);

void defos_set_cursor(DefosCursor cursor_type)
{
    Cursor cursor = XCreateFontCursor(disp, get_cursor(cursor_type));
    XDefineCursor(disp, win, cursor);
    if (has_custom_cursor) { XFreeCursor(disp, custom_cursor); }
    custom_cursor = cursor;
    has_custom_cursor = true;
}

void defos_reset_cursor()
{
    if (has_custom_cursor)
    {
        XUndefineCursor(disp, win);
        XFreeCursor(disp, custom_cursor);
        has_custom_cursor = false;
    }
}

static unsigned int get_cursor(DefosCursor cursor)
{
    switch (cursor)
    {
    case DEFOS_CURSOR_ARROW:
        return XC_left_ptr;
    case DEFOS_CURSOR_CROSSHAIR:
        return XC_tcross;
    case DEFOS_CURSOR_HAND:
        return XC_hand2;
    case DEFOS_CURSOR_IBEAM:
        return XC_xterm;
    default:
        return XC_left_ptr;
    }
}

static bool is_window_visible(Window window)
{
    XWindowAttributes attributes;
    XGetWindowAttributes(disp, window, &attributes);
    return attributes.map_state == IsViewable;
}

static const XRRModeInfo* get_mode_info(const XRRScreenResources* screenResources, RRMode id){
    for (int i = 0; i < screenResources->nmode; i++){
        if (screenResources->modes[i].id == id){
            return screenResources->modes + i;
        }
    }
    return NULL;
}

static double compute_refresh_rate(const XRRModeInfo* modeInfo)
{
    if (!modeInfo->hTotal || !modeInfo->vTotal) {
        return 0;
    }
    return ((double)modeInfo->dotClock / ((double)modeInfo->hTotal * (double)modeInfo->vTotal));
}

extern void defos_get_displays(dmArray<DisplayInfo> &displayList)
{
    XRRScreenResources *screenResources = XRRGetScreenResourcesCurrent(disp, win);
    unsigned long bpp = (long)DefaultDepth(disp, screen);

    displayList.OffsetCapacity(screenResources->ncrtc);
    for (int i = 0; i < screenResources->ncrtc; i++)
    {
        RRCrtc crtc = screenResources->crtcs[i];
        DisplayInfo display;

        XRRCrtcInfo *crtcInfo = XRRGetCrtcInfo(disp, screenResources, crtc);
        const XRRModeInfo * modeInfo = get_mode_info(screenResources, crtcInfo->mode);

        if (!modeInfo)
        {
            XRRFreeCrtcInfo(crtcInfo);
            continue;
        }

        bool isMirror = false;
        for (int j = 0; j < displayList.Size(); j++)
        {
            DisplayInfo &otherDisplay = displayList[j];
            if (otherDisplay.bounds.x == crtcInfo->x
                && otherDisplay.bounds.y == crtcInfo->y
                && otherDisplay.bounds.w == crtcInfo->width
                && otherDisplay.bounds.h == crtcInfo->height)
            {
                isMirror = true;
                break;
            }
        }

        if (isMirror)
        {
            XRRFreeCrtcInfo(crtcInfo);
            continue;
        }

        display.id = (DisplayID)crtc;
        display.bounds.x = crtcInfo->x;
        display.bounds.y = crtcInfo->y;
        display.bounds.w = crtcInfo->width;
        display.bounds.h = crtcInfo->height;
        display.mode.width = modeInfo->width;
        display.mode.height = modeInfo->height;
        display.mode.refresh_rate = compute_refresh_rate(modeInfo);
        display.mode.bits_per_pixel = bpp;
        display.mode.scaling_factor = (double)display.mode.width / (double)display.bounds.w;
        display.name = NULL;

        if (crtcInfo->noutput > 0)
        {
            XRROutputInfo *outputInfo = XRRGetOutputInfo(disp, screenResources, crtcInfo->outputs[0]);
            if (outputInfo->name)
            {
                display.name = (char*)malloc(outputInfo->nameLen + 1);
                strcpy(display.name, outputInfo->name);
            }
            XRRFreeOutputInfo(outputInfo);
        }

        displayList.Push(display);

        XRRFreeCrtcInfo(crtcInfo);
    }

    XRRFreeScreenResources(screenResources);
}

extern void defos_get_display_modes(DisplayID displayID, dmArray<DisplayModeInfo> &modeList)
{
    RRCrtc crtc = (RRCrtc)displayID;

    XRRScreenResources *screenResources = XRRGetScreenResourcesCurrent(disp, win);
    XRRCrtcInfo *crtcInfo = XRRGetCrtcInfo(disp, screenResources, crtc);

    if (crtcInfo->noutput <= 0)
    {
        XRRFreeCrtcInfo(crtcInfo);
        XRRFreeScreenResources(screenResources);
        return;
    }

    RROutput output = crtcInfo->outputs[0];
    XRROutputInfo *outputInfo = XRRGetOutputInfo(disp, screenResources, output);

    unsigned long bpp = (long)DefaultDepth(disp, screen);

    const XRRModeInfo *currentModeInfo = get_mode_info(screenResources, crtcInfo->mode);
    double scaling_factor = (double)currentModeInfo->width / (double)crtcInfo->width;

    modeList.OffsetCapacity(outputInfo->nmode);
    for (int i = 0; i < outputInfo->nmode; i++){
        const XRRModeInfo *modeInfo = get_mode_info(screenResources, outputInfo->modes[i]);

        DisplayModeInfo mode;
        mode.width = modeInfo->width;
        mode.height = modeInfo->height;
        mode.refresh_rate = compute_refresh_rate(modeInfo);
        mode.bits_per_pixel = bpp;
        mode.scaling_factor = scaling_factor;

        modeList.Push(mode);
    }

    XRRFreeOutputInfo(outputInfo);
    XRRFreeScreenResources(screenResources);
}

extern DisplayID defos_get_current_display()
{
    RROutput output = XRRGetOutputPrimary(disp, win);

    XRRScreenResources *screenResources = XRRGetScreenResourcesCurrent(disp, win);
    XRROutputInfo *outputInfo = XRRGetOutputInfo(disp, screenResources, output);

    RRCrtc crtc = outputInfo->crtc;

    XRRFreeOutputInfo(outputInfo);
    XRRFreeScreenResources(screenResources);

    return (DisplayID)crtc;
}

extern void  defos_set_window_icon(const char *icon_path)
{
}

extern char* defos_get_bundle_root()
{
    const char *path = ".";
    char *result = (char*)malloc(strlen(path) + 1);
    strcpy(result, path);
    return result;
}

extern void defos_get_parameters(dmArray<char*> &parameters)
{
}

//from glfw/x11_window.c
static void send_message(Window &window, Atom type, long a, long b, long c, long d, long e)
{
    XEvent event;
    memset(&event, 0, sizeof(event));

    event.type = ClientMessage;
    event.xclient.window = window;
    event.xclient.format = 32;
    event.xclient.message_type = type;
    event.xclient.data.l[0] = a;
    event.xclient.data.l[1] = b;
    event.xclient.data.l[2] = c;
    event.xclient.data.l[3] = d;
    event.xclient.data.l[4] = e;

    XSendEvent(disp, root, False, SubstructureNotifyMask | SubstructureRedirectMask, &event);
}

#endif
