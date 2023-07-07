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
 * DATA                                                                       *
 *****************************************************************************/
struct termios term_bak; // termios backup used restore terminal to on exit

/******************************************************************************
 * CODE                                                                       *
 *****************************************************************************/

/**
 * @brief prints error message and terminates
 * @param cause - string containing a brief error message
 */
void exception(const char *cause)
{
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

int main(void)
{
    enable_raw_mode();
    char c = '\0';
    while (true)
    {
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
            exception("read");
        if (iscntrl(c))          // checks if c is a control character
            printf("%d\r\n", c); // print ASCII number
        else                     // else prints ASCII number and character
            printf("%d ('%c')\r\n", c, c);
        if (c == 'q')
            break;
    }
    return 0;
}