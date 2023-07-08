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

/******************************************************************************
 * DEFINE                                                                     *
 *****************************************************************************/
#define CTRL_KEY(k) ((k)&0x1f)
#define CUP "\x1b[H"       // Cursor Position
#define TERM_CLS "\x1b[2J" // Escape Character

/******************************************************************************
 * DATA                                                                       *
 *****************************************************************************/
struct termios term_bak; // termios backup used restore terminal to on exit

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
 * @brief restores termios to backup using `term_bak`
 * @details used on program termination to disable raw modes
 * @throw exception on failure to set terminal params
 */
void restore_termios()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_bak) == -1)
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
    if (tcgetattr(STDIN_FILENO, &term_bak) == -1)
        exception("tcgetattr");
    struct termios raw = term_bak;
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

int main(void)
{
    enable_raw_mode();
    char c = '\0';
    while (true)
    {
        editorClearScreen();
        editorProcessKeypress();
    }
    return 0;
}