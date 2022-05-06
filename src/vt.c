#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <X11/keysym.h>
#include <X11/X.h>
#include "render.h"
#include "util.h"
#include "vt.h"
#include "vterm.h"

#define MAX_COLS 512
#define MAX_ROWS 256

void vt_init(PiWorldTerm *pwt, int cols, int rows) {
    pwt->cols = MIN(cols, MAX_COLS);
    pwt->rows = MIN(rows, MAX_ROWS);
    pwt->cols = MAX(cols, 1);
    pwt->rows = MAX(rows, 1);
    pwt->vt = vterm_new(pwt->rows, pwt->cols);
    vterm_set_utf8(pwt->vt, 1);
    pwt->vts = vterm_obtain_screen(pwt->vt);
    vterm_screen_reset(pwt->vts, 1);

    // termios setup taken from pangoterm
    struct termios termios = {
        .c_iflag = ICRNL|IXON,
        .c_oflag = OPOST|ONLCR
#ifdef TAB0
            |TAB0
#endif
        ,
        .c_cflag = CS8|CREAD,
        .c_lflag = ISIG|ICANON|IEXTEN|ECHO|ECHOE|ECHOK,
        /* c_cc later */
    };

#ifdef IUTF8
    termios.c_iflag |= IUTF8;
#endif
#ifdef NL0
    termios.c_oflag |= NL0;
#endif
#ifdef CR0
    termios.c_oflag |= CR0;
#endif
#ifdef BS0
    termios.c_oflag |= BS0;
#endif
#ifdef VT0
    termios.c_oflag |= VT0;
#endif
#ifdef FF0
    termios.c_oflag |= FF0;
#endif
#ifdef ECHOCTL
    termios.c_lflag |= ECHOCTL;
#endif
#ifdef ECHOKE
    termios.c_lflag |= ECHOKE;
#endif

    cfsetspeed(&termios, 38400);

    termios.c_cc[VINTR]    = 0x1f & 'C';
    termios.c_cc[VQUIT]    = 0x1f & '\\';
    termios.c_cc[VERASE]   = 0x7f;
    termios.c_cc[VKILL]    = 0x1f & 'U';
    termios.c_cc[VEOF]     = 0x1f & 'D';
    termios.c_cc[VEOL]     = _POSIX_VDISABLE;
    termios.c_cc[VEOL2]    = _POSIX_VDISABLE;
    termios.c_cc[VSTART]   = 0x1f & 'Q';
    termios.c_cc[VSTOP]    = 0x1f & 'S';
    termios.c_cc[VSUSP]    = 0x1f & 'Z';
    termios.c_cc[VREPRINT] = 0x1f & 'R';
    termios.c_cc[VWERASE]  = 0x1f & 'W';
    termios.c_cc[VLNEXT]   = 0x1f & 'V';
    termios.c_cc[VMIN]     = 1;
    termios.c_cc[VTIME]    = 0;

    struct winsize size = { pwt->rows, pwt->cols, 0, 0 };

    /* Save the real stderr before forkpty so we can still print errors to it if
     * we fail
     */
    int stderr_save_fileno = dup(2);

    pid_t kid = forkpty(&pwt->master, NULL, &termios, &size);
    if (kid == 0) {
        fcntl(stderr_save_fileno, F_SETFD, fcntl(stderr_save_fileno, F_GETFD) | FD_CLOEXEC);
        FILE *stderr_save = fdopen(stderr_save_fileno, "a");

        /* Restore the ISIG signals back to defaults */
        signal(SIGINT,  SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGSTOP, SIG_DFL);
        signal(SIGCONT, SIG_DFL);

        /* Do not free 'term', it is part of the environment */
        putenv("TERM=xterm");

        char *shell = getenv("SHELL");
        char *args[2] = { shell, NULL };
        execvp(shell, args);
        fprintf(stderr_save, "Cannot exec(%s) - %s\n", shell, strerror(errno));
        _exit(1);
    }

    close(stderr_save_fileno);
    fcntl(pwt->master, F_SETFL, fcntl(pwt->master, F_GETFL) | O_NONBLOCK);
}

void vt_deinit(PiWorldTerm *pwt)
{
    vterm_free(pwt->vt);
}

void vt_set_size(PiWorldTerm *pwt, int cols, int rows)
{
    if (pwt->cols == cols && pwt->rows == rows) {
        return;
    }
    if (cols <= 0 || rows <= 0) {
        printf("Invalid terminal size: %d %d\n", cols, rows);
        return;
    }
    pwt->cols = MIN(cols, MAX_COLS);
    pwt->rows = MIN(rows, MAX_ROWS);
    vterm_set_size(pwt->vt, pwt->rows, pwt->cols);

    struct winsize size = { pwt->rows, pwt->cols, 0, 0 };
    ioctl(pwt->master, TIOCSWINSZ, &size);
}

void vt_draw(PiWorldTerm *pwt, float x, float y, float scale)
{
    float ty = y - (FONT_HEIGHT / 2) * scale;
    float tx = x + (FONT_WIDTH / 2) * scale;

    // Render lines from VT
    char line[1024];
    for (int row=0; row < pwt->rows; row++) {
        for (int col=0; col < pwt->cols; col++) {
            VTermPos pos = {.row = row, .col = col};
            VTermScreenCell cell;
            vterm_screen_get_cell(pwt->vts, pos, &cell);
            line[col] = cell.chars[0];
            if (cell.chars[0] == 0) {
                // Fill in a skipped space
                line[col] = ' ';
            }
        }
        line[pwt->cols] = '\0';
        render_text(ALIGN_LEFT, tx, ty - (row * FONT_HEIGHT * scale), FONT_WIDTH * scale, line);
    }

    // Render text cursor
    VTermPos cursorpos;
    VTermState *state = vterm_obtain_state(pwt->vt);
    vterm_state_get_cursorpos(state, &cursorpos);
    float cx = cursorpos.col * FONT_WIDTH * scale;
    float cy = ty - cursorpos.row * FONT_HEIGHT * scale;
    glClear(GL_DEPTH_BUFFER_BIT);
    render_text_cursor(cx, cy);
}

int vt_process(PiWorldTerm *pwt)
{
    /* Linux kernel's PTY buffer is a fixed 4096 bytes (1 page) so there's
     * never any point read()ing more than that
     */
    char buffer[4096];

    ssize_t bytes = read(pwt->master, buffer, sizeof buffer);

    if (bytes == -1 && errno == EAGAIN) {
        return true;
    }

    if (bytes == 0 || (bytes == -1 && errno == EIO)) {
        return false;
    }
    if (bytes < 0) {
        fprintf(stderr, "read(master) failed - %s\n", strerror(errno));
        return false;
    }

    vterm_input_write(pwt->vt, buffer, bytes);

    return true;
}

static void term_flush_output(PiWorldTerm *pwt)
{
    size_t bufflen = vterm_output_get_buffer_current(pwt->vt);
    if (bufflen) {
        char buffer[bufflen];
        bufflen = vterm_output_read(pwt->vt, buffer, bufflen);
        write(pwt->master, buffer, bufflen);
    }
}

void vt_handle_key_press(PiWorldTerm *pwt, int mods, int keysym)
{
    if (keysym >= XK_Shift_L && keysym <= XK_Hyper_R) {
        return;
    }
    VTermModifier mod = VTERM_MOD_NONE;
    if (mods & ShiftMask)
        mod |= VTERM_MOD_SHIFT;
    if (mods & ControlMask)
        mod |= VTERM_MOD_CTRL;
    if (mods & Mod1Mask)
        mod |= VTERM_MOD_ALT;

    if (keysym >= XK_F1 && keysym <= XK_F35) {
        vterm_keyboard_key(pwt->vt, VTERM_KEY_FUNCTION(keysym - XK_F1 + 1), mod);
    } else {
        switch (keysym) {
        case XK_Return:
            vterm_keyboard_key(pwt->vt, VTERM_KEY_ENTER, mod);
            break;
        case XK_Tab:
            vterm_keyboard_key(pwt->vt, VTERM_KEY_TAB, mod);
            break;
        case XK_BackSpace:
            vterm_keyboard_key(pwt->vt, VTERM_KEY_BACKSPACE, mod);
            break;
        case XK_Escape:
            vterm_keyboard_key(pwt->vt, VTERM_KEY_ESCAPE, mod);
            break;
        case XK_Up:
            vterm_keyboard_key(pwt->vt, VTERM_KEY_UP, mod);
            break;
        case XK_Down:
            vterm_keyboard_key(pwt->vt, VTERM_KEY_DOWN, mod);
            break;
        case XK_Left:
            vterm_keyboard_key(pwt->vt, VTERM_KEY_LEFT, mod);
            break;
        case XK_Right:
            vterm_keyboard_key(pwt->vt, VTERM_KEY_RIGHT, mod);
            break;
        case XK_Insert:
            vterm_keyboard_key(pwt->vt, VTERM_KEY_INS, mod);
            break;
        case XK_Delete:
            vterm_keyboard_key(pwt->vt, VTERM_KEY_DEL, mod);
            break;
        case XK_Home:
            vterm_keyboard_key(pwt->vt, VTERM_KEY_HOME, mod);
            break;
        case XK_End:
            vterm_keyboard_key(pwt->vt, VTERM_KEY_END, mod);
            break;
        case XK_Page_Up:
            vterm_keyboard_key(pwt->vt, VTERM_KEY_PAGEUP, mod);
            break;
        case XK_Page_Down:
            vterm_keyboard_key(pwt->vt, VTERM_KEY_PAGEDOWN, mod);
            break;

        // Numpad
        case XK_Num_Lock:
            // ignore
            break;
        case XK_KP_Enter:
            vterm_keyboard_key(pwt->vt, VTERM_KEY_ENTER, mod);
            break;
        case XK_KP_Divide:
            vterm_keyboard_unichar(pwt->vt, XK_slash, mod);
            break;
        case XK_KP_Multiply:
            vterm_keyboard_unichar(pwt->vt, XK_asterisk, mod);
            break;
        case XK_KP_Subtract:
            vterm_keyboard_unichar(pwt->vt, XK_minus, mod);
            break;
        case XK_KP_Home:
            vterm_keyboard_unichar(pwt->vt, XK_7, mod);
            break;
        case XK_KP_Up:
            vterm_keyboard_unichar(pwt->vt, XK_8, mod);
            break;
        case XK_KP_Page_Up:
            vterm_keyboard_unichar(pwt->vt, XK_9, mod);
            break;
        case XK_KP_Add:
            vterm_keyboard_unichar(pwt->vt, XK_plus, mod);
            break;
        case XK_KP_Left:
            vterm_keyboard_unichar(pwt->vt, XK_4, mod);
            break;
        case XK_KP_Begin:
            vterm_keyboard_unichar(pwt->vt, XK_5, mod);
            break;
        case XK_KP_Right:
            vterm_keyboard_unichar(pwt->vt, XK_6, mod);
            break;
        case XK_KP_End:
            vterm_keyboard_unichar(pwt->vt, XK_1, mod);
            break;
        case XK_KP_Down:
            vterm_keyboard_unichar(pwt->vt, XK_2, mod);
            break;
        case XK_KP_Page_Down:
            vterm_keyboard_unichar(pwt->vt, XK_3, mod);
            break;
        case XK_KP_Insert:
            vterm_keyboard_unichar(pwt->vt, XK_0, mod);
            break;
        case XK_KP_Delete:
            vterm_keyboard_unichar(pwt->vt, XK_period, mod);
            break;

        default:
            vterm_keyboard_unichar(pwt->vt, keysym, mod);
            break;
        }
    }

    term_flush_output(pwt);
}
