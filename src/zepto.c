/******************************************************************************
 * INCLUDES                                                                   *
 *****************************************************************************/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

/******************************************************************************
 * DEFINE                                                                     *
 *****************************************************************************/
#define CTRL_KEY(k) ((k)&0x1f)
#define CUP "\x1b[H"       // Cursor Position
#define TERM_CLS "\x1b[2J" // Escape Character

/******************************************************************************
 * DATA                                                                       *
 *****************************************************************************/
struct editorConfig
{
    int screenrows;
    int screencols;
    struct termios term_bak; // termios backup used restore terminal to on exit
};
struct editorConfig E;

/******************************************************************************
 * CODE                                                                       *
 *****************************************************************************/
void editorClearScreen()
{
    write(STDOUT_FILENO, TERM_CLS, 4);
    write(STDOUT_FILENO, CUP, 3);
}

/**
 * @brief prints error message and terminates
 * @param cause - string containing a brief error message
 */
void exception(const char *cause)
{
    editorClearScreen();
    perror(cause);
    exit(EXIT_FAILURE);
}

/**
 * @brief wait for one key press and return
 * @return key that was pressed
 * @throw exception on fail to read
 */
char editorReadKey()
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
            exception("read");
    }
    return c;
}
int getCursorPosition(int *numRow, int *numCol)
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

int getTerminalSize(int *numRow, int *numCol)
{
    struct winsize W;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &W) == -1 || !W.ws_col)
    {
        // fall back in case ioctl() fails
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1; // fail
        return getCursorPosition(numRow, numCol);
    }
    else
    {
        *numRow = W.ws_row;
        *numCol = W.ws_col;
        return 0; // sucess
    }
}

/**
 * @brief restores termios to backup using `term_bak`
 * @details used on program termination to disable raw modes
 * @throw exception on failure to set terminal params
 */
void restore_termios()
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
    atexit(restore_termios);
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
 * @brief wait for key press and handle
 */
void editorProcessKeypress()
{
    char c = editorReadKey();
    switch (c)
    {
    case CTRL_KEY('q'):
        editorClearScreen();
        exit(0);
        break;
    }
}
void drawLeftTilde()
{
    int y;
    for (y = 0; y < E.screenrows; y++)
    {
        write(STDOUT_FILENO, "~", 1);
        if (y < E.screenrows - 1)
            write(STDOUT_FILENO, "\r\n", 2);
    }
}
int init()
{
    if (getTerminalSize(&E.screenrows, &E.screencols) == -1)
    {
        exception("getTerminalSize");
        return -1;
    }
    return 1;
}

int main(void)
{
    enable_raw_mode();
    init();
    char c = '\0';
    while (true)
    {
        editorClearScreen();
        drawLeftTilde();
        write(STDOUT_FILENO, "\x1b[H", 3); // reposition cursor

        editorProcessKeypress();
    }
    return 0;
}