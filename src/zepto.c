/******************************************************************************
 * INCLUDES                                                                   *
 *****************************************************************************/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>

/******************************************************************************
 * DEFINES                                                                    *
 *****************************************************************************/
#define ZEPTO_VERSION "0.0.1"
#define ZEPTO_TAB_STOP 8
#define CTRL_KEY(k) ((k)&0x1f)
#define CUP "\x1b[H"       // Cursor Position
#define TERM_CLS "\x1b[2J" // Escape Character
#define ERASE_IN_LINE "\x1b[K"

enum editorKey
{
    ARROW_LEFT = 1000, // choose a representation that is outside normal ASCII
    ARROW_RIGHT,       // the rest increments from 1000
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};
/******************************************************************************
 * DATA                                                                       *
 *****************************************************************************/
typedef struct erow
{
    int size;
    int rsize;
    char *chars;
    char *render;
} erow; // editor row
struct editorConfig
{
    int cx, cy;
    int rowoff;
    int coloff;
    int screenrows;
    int screencols;
    struct termios term_bak; // termios backup used restore terminal to on exit
    int numrows;
    erow *row;
};
struct editorConfig E;

/******************************************************************************
 * TERMINAL                                                                   *
 *****************************************************************************/

/**
 * @brief prints error message and terminates
 * @param cause - string containing a brief error message
 */
void exception(const char *cause)
{
    write(STDOUT_FILENO, TERM_CLS, 4);
    write(STDOUT_FILENO, CUP, 3);
    perror(cause);
    exit(EXIT_FAILURE);
}
/**
 * @brief restores termios to backup using `term_bak`
 * @details used on program termination to disable raw modes
 * @throw exception on failure to set terminal params
 */
void disable_raw_mode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.term_bak) == -1)
        exception("tcsetattr");
}

/**
 * @brief enables raw mode
 * @details disables: echo, canonical mode, interrupts, ctrl-v, software control
 * flow (ctrl-s, ctrl-q), ctrl-m, output processing; and sets a timeout for
 * read()
 * @throw exception if fail to get or set terminal attributes
 */
void enable_raw_mode()
{
    if (tcgetattr(STDIN_FILENO, &E.term_bak) == -1)
        exception("tcgetattr");
    struct termios raw = E.term_bak;
    atexit(disable_raw_mode);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        exception("tcsetattr");
}
/**
 * @brief wait for one key press and return
 * @return key that was pressed
 * @throw exception on fail to read
 */
int editor_read_key()
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
            exception("read");
    }
    if (c == '\x1b') // check for escape characters
    {
        char seq[3];
        if ((read(STDIN_FILENO, &seq[0], 1) != 1) || (read(STDIN_FILENO, &seq[1], 1) != 1))
            return '\x1b'; // user entered ESC
        if (seq[0] == '[')
        {
            if ('0' <= seq[1] && seq[1] <= '9')
            {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return '\x1b';
                if (seq[2] == '~')
                {
                    switch (seq[1])
                    {
                    case '1':
                        return HOME_KEY;
                    case '3':
                        return DEL_KEY;
                    case '4':
                        return END_KEY;
                    case '5':
                        return PAGE_UP;
                    case '6':
                        return PAGE_DOWN;
                    case '7':
                        return HOME_KEY;
                    case '8':
                        return END_KEY;
                    }
                }
            }
            else
            {
                switch (seq[1])
                {
                case 'A':
                    return ARROW_UP;
                case 'B':
                    return ARROW_DOWN;
                case 'C':
                    return ARROW_RIGHT;
                case 'D':
                    return ARROW_LEFT;
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
                }
            }
        }
        else if (seq[0] == 'O')
        {
            switch (seq[1])
            {
            case 'H':
                return HOME_KEY;
            case 'F':
                return END_KEY;
            }
        }
        return '\x1b';
    }
    return c;
}
int get_cursor_position(int *numRow, int *numCol)
{
    char buf[32];
    unsigned int i = 0;
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;
    while (i < sizeof(buf) - 1)
    {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';
    printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);
    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;
    if (sscanf(&buf[2], "%d;%d", numRow, numCol) != 2)
        return -1;
    return 0;
}

int get_terminal_size(int *numRow, int *numCol)
{
    struct winsize W;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &W) == -1 || !W.ws_col)
    {
        // fall back in case ioctl() fails
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1; // fail
        return get_cursor_position(numRow, numCol);
    }
    else
    {
        *numRow = W.ws_row;
        *numCol = W.ws_col;
        return 0; // sucess
    }
}
/******************************************************************************
 * ROW OPERATIONS                                                             *
 *****************************************************************************/
void editor_update_row(erow *row)
{
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++)
        if (row->chars[j] == '\t')
            tabs++;
    free(row->render);
    row->render = malloc(row->size + tabs * (ZEPTO_TAB_STOP - 1) + 1);

    int idx = 0;
    for (j = 0; j < row->size; j++)
    {
        if (row->chars[j] == '\t')
        {
            row->render[idx++] = ' ';
            while (idx % ZEPTO_TAB_STOP != 0)
                row->render[idx++] = ' ';
        }
        else
        {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void editor_append_row(char *s, size_t len)
{
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.numrows++;
    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editor_update_row(&E.row[at]);
}
/******************************************************************************
 * FILE I/O                                                                   *
 *****************************************************************************/

void editor_open(char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
        exception("fopen");
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1)
    {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
            linelen--;
        editor_append_row(line, linelen);
    }
    free(line);
    fclose(fp);
}
/******************************************************************************
 * APPEND BUFFER                                                              *
 *****************************************************************************/
struct appendBuffer
{
    char *b;
    int len;
};

#define ABUF_INIT \
    {             \
        NULL, 0   \
    }

void ab_append(struct appendBuffer *ab, const char *s, int len)
{
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL)
        return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}
void ab_free(struct appendBuffer *ab)
{
    free(ab->b);
}
/******************************************************************************
 * OUTPUT                                                                     *
 *****************************************************************************/
void editor_scroll()
{
    if (E.cy < E.rowoff)
        E.rowoff = E.cy;
    if (E.cy >= E.rowoff + E.screenrows)
        E.rowoff = E.cy - E.screenrows + 1;
    if (E.cx < E.coloff)
        E.coloff = E.cx;
    if (E.cx >= E.coloff + E.screencols)
        E.coloff = E.cx - E.screencols + 1;
}
void editor_draw_rows(struct appendBuffer *ab)
{
    int y;
    for (y = 0; y < E.screenrows; y++)
    {
        int file_row = y + E.rowoff;
        if (file_row >= E.numrows)
        {
            if (!E.numrows && y == E.screenrows / 3)
            {
                char welcome[80];
                int welcomelen = snprintf(welcome,
                                          sizeof(welcome), "Zepto editor -- version %s", ZEPTO_VERSION);
                if (welcomelen > E.screencols)
                    welcomelen = E.screencols;
                int padding = (E.screencols - welcomelen) / 2;
                if (padding)
                {
                    ab_append(ab, "~", 1);
                    padding--;
                }
                while (padding--)
                    ab_append(ab, " ", 1);
                ab_append(ab, welcome, welcomelen);
            }
            else
                ab_append(ab, "~", 1);
        }
        else
        {
            int len = E.row[file_row].rsize - E.coloff;
            len = len > 0 ? len : 0;
            if (len > E.screencols)
                len = E.screencols;
            ab_append(ab, &E.row[file_row].render[E.coloff], len);
        }
        ab_append(ab, ERASE_IN_LINE, 3);
        if (y < E.screenrows - 1)
            ab_append(ab, "\r\n", 2);
    }
}

void editor_refresh_screen()
{
    editor_scroll();
    struct appendBuffer ab = ABUF_INIT;
    ab_append(&ab, "\x1b[?25l", 6);
    ab_append(&ab, CUP, 3);
    editor_draw_rows(&ab);
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy - E.rowoff + 1, E.cx - E.coloff + 1);
    ab_append(&ab, buf, strlen(buf));
    ab_append(&ab, "\x1b[?25h", 6);
    write(STDOUT_FILENO, ab.b, ab.len);
    ab_free(&ab);
}

/******************************************************************************
 * INPUT                                                                      *
 *****************************************************************************/

void editor_move_cursor(int key)
{
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    switch (key)
    {
    case ARROW_LEFT:
        if (E.cx != 0)
            E.cx--;
        else if (E.cy > 0)
        {
            E.cy--;
            E.cx = E.row[E.cy].size;
        }
        break;
    case ARROW_RIGHT:
        if (row && E.cx < row->size)
            E.cx++;
        else if (row && E.cx == row->size)
        {
            E.cy++;
            E.cx = 0;
        }
        break;
    case ARROW_UP:
        if (E.cy != 0)
            E.cy--;
        break;
    case ARROW_DOWN:
        if (E.cy < E.numrows)
            E.cy++;
        break;
    }
    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen)
        E.cx = rowlen;
}
/**
 * @brief wait for key press and handle
 */
void editor_process_keypress()
{
    int c = editor_read_key();
    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, TERM_CLS, 4);
        write(STDOUT_FILENO, CUP, 3);
        exit(0);
        break;
    case HOME_KEY:
        E.cx = 0;
        break;
    case END_KEY:
        E.cx = E.screencols - 1;
        break;
    case PAGE_UP:
    case PAGE_DOWN:
    { // braces required to declare variable inside case
        int t = E.screenrows;
        while (t--)
            editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    }
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        editor_move_cursor(c);
        break;
    }
}

int initialize_editor()
{
    E.cx = 0;
    E.cy = 0;
    E.numrows = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.row = NULL;

    if (get_terminal_size(&E.screenrows, &E.screencols) == -1)
    {
        exception("get_terminal_size");
        return -1;
    }
    return 1;
}

int main(int argc, char *argv[])
{
    enable_raw_mode();
    initialize_editor();
    if (argc >= 2)
    {
        editor_open(argv[1]);
    }
    char c = '\0';
    while (true)
    {
        editor_refresh_screen();
        editor_process_keypress();
    }
    return 0;
}