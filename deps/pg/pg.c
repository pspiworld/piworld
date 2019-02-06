
#include <assert.h>
#include "pg.h"
#include "pg_time.h"

#ifdef MESA
#include "pg_x11.h"
#else
#include "pg_pi.h"
#endif

typedef struct
{
    uint32_t window_x;
    uint32_t window_y;
    uint32_t window_width;
    uint32_t window_height;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
} PG_STATE_T;

static PG_STATE_T _pg_state, *pg_state=&_pg_state;


static void init_gles2(PG_STATE_T *state, char *title)
{
    EGLBoolean result;
    EGLint num_config;
    static EGLNativeWindowType nativewindow;

    EGLint context_attribs[4];
    context_attribs[0] = EGL_CONTEXT_CLIENT_VERSION;
    context_attribs[1] = 2;
    context_attribs[2] = EGL_NONE;

    static const EGLint attribute_list[] =
    {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 1,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    EGLConfig config;

    // get an EGL display connection
    state->display = eglGetDisplay(get_egl_display_id());
    assert(state->display!=EGL_NO_DISPLAY);

    // initialize the EGL display connection
    result = eglInitialize(state->display, NULL, NULL);
    assert(EGL_FALSE != result);

    // get an appropriate EGL frame buffer configuration
    result = eglChooseConfig(state->display, attribute_list, &config, 1,
                             &num_config);
    assert(EGL_FALSE != result);

    eglBindAPI(EGL_OPENGL_ES_API);

    // create an EGL rendering context
    state->context = eglCreateContext(state->display, config, EGL_NO_CONTEXT,
                                      context_attribs);
    assert(state->context!=EGL_NO_CONTEXT);

    // create an EGL window surface
    nativewindow = get_egl_window_id(config, state->display,
                                     &state->window_x, &state->window_y,
                                     &state->window_width,
                                     &state->window_height, title);

    state->surface = eglCreateWindowSurface(state->display, config,
                                            nativewindow, NULL);
    assert(state->surface != EGL_NO_SURFACE);

    // connect the context to the surface
    result = eglMakeCurrent(state->display, state->surface, state->surface,
                            state->context);
    assert(EGL_FALSE != result);

    // set background color
    glClearColor(0.15f, 0.25f, 0.35f, 1.0f);
}

void pg_print_info()
{
    // print EGL info
    printf("\n");
    printf("EGL_VERSION = %s\n", eglQueryString(pg_state->display, EGL_VERSION));
    printf("EGL_VENDOR = %s\n", eglQueryString(pg_state->display, EGL_VENDOR));
    printf("EGL_EXTENSIONS = %s\n",
           eglQueryString(pg_state->display, EGL_EXTENSIONS));
    printf("EGL_CLIENT_APIS = %s\n",
           eglQueryString(pg_state->display, EGL_CLIENT_APIS));

    // print GL info
    printf("\n");
    printf("GL_VERSION = %s\n", glGetString(GL_VERSION));
    printf("GL_VENDOR = %s\n", glGetString(GL_VENDOR));
    printf("GL_RENDERER = %s\n", glGetString(GL_RENDERER));
    printf("GL_SHADING_LANGUAGE_VERSION = %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("GL_EXTENSIONS = %s\n", glGetString(GL_EXTENSIONS));
}

void pg_start(char *title, int x, int y, int width, int height)
{
    pg_time_init();

    // Clear application state
    memset(pg_state, 0, sizeof(*pg_state));

    // Start OGLES
    pg_state->window_x = x;
    pg_state->window_y = y;
    pg_state->window_width = width;
    pg_state->window_height = height;
    init_gles2(pg_state, title);
}

void pg_swap_buffers()
{
    eglSwapBuffers(pg_state->display, pg_state->surface);
}

void pg_swap_interval(int interval)
{
    eglSwapInterval(pg_state->display, interval);
}

void pg_get_window_size(int *width, int *height)
{
    *width = pg_state->window_width;
    *height = pg_state->window_height;
}

void pg_set_window_geometry(int x, int y, int width, int height)
{
    #ifndef MESA
    // Call extra setup function for brcm driver
    pi_set_window_geometry(&x, &y, &width, &height);
    #endif
    pg_state->window_x = x;
    pg_state->window_y = y;
    pg_state->window_width = width;
    pg_state->window_height = height;
}

