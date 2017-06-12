#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <ctype.h>
#include <locale.h>
#include <curses.h>


#define BUFFER_SIZE ((int)512)
static char buffer[BUFFER_SIZE] = { '\0' };
static int buflen = 0;
static int bufidx = 0;
static int cx = 0, cy = 0;

static inline void refreshTextBox(void);

static inline void initialize_ncurses(void)
{
	initscr();
	noecho();
	timeout(0);
	keypad(stdscr, TRUE);
	signal(SIGWINCH, (void(*)(int))refreshTextBox);
}


static inline void terminate_ncurses(void)
{
	endwin();
}


static inline void refreshTextBox(void)
{
	clear();
	move(0, 0);
	printw(buffer);
	move(cy, cx);
	refresh();
}


static inline void clearTextBox(void)
{
	cy = cx = 0;
	buflen = 0;
	bufidx = 0;
	clear();
	move(0, 0);
	refresh();
}


static inline void moveCursorLeft(void)
{
	if (cx > 0 || cy > 0) {
		--bufidx;
		--cx;
		if (cx < 0) {
			cx = getmaxx(stdscr) - 1;
			--cy;
		}
		move(cy, cx);
	}
}


static inline void moveCursorRight(void)
{
	if (bufidx < buflen) {
		++bufidx;
		++cx;
		if (cx >= getmaxx(stdscr)) {
			cx = 0;
			++cy;
		}
		move(cy, cx);
	}
}


static inline void moveCursorHome(void)
{
	if (bufidx != 0) {
		bufidx = 0;
		cy = cx = 0;
		move(cy, cx);
	}
}


static inline void moveCursorEnd(void)
{
	if (bufidx < buflen) {
		bufidx = buflen;
		cx = buflen % getmaxx(stdscr);
		cy = buflen / getmaxx(stdscr);
		move(cy, cx);
	}
}


static inline bool updateTextBox(void)
{
	const int c = getch();

	switch (c) {
	case 10: // also enter (ascii) [fall]
	case KEY_ENTER: // submit msg
		return true;
	case KEY_LEFT:
		moveCursorLeft();
		return false;
	case KEY_RIGHT:
		moveCursorRight();
		return false;
	case 127: // also backspace (ascii) [fall]
	case KEY_BACKSPACE:
		if (bufidx > 0) {
			memmove(&buffer[bufidx - 1], &buffer[bufidx], buflen - bufidx);
			buffer[--buflen] = '\0';
			moveCursorLeft();
			refreshTextBox();
		}
		return false;
	case KEY_HOME:
		moveCursorHome();
		return false;
	case KEY_END:
		moveCursorEnd();
		return false;
	}

	if (isascii(c) && buflen < BUFFER_SIZE) {
		if (bufidx < buflen)
			memmove(&buffer[bufidx + 1], &buffer[bufidx], buflen - bufidx);
		buffer[bufidx] = (char) c;
		buffer[++buflen] = '\0';
		moveCursorRight();
		refreshTextBox();
	}

	return false;
}


int main(void)
{
	initialize_ncurses();

	for (;;) {
		if (updateTextBox()) {
			if (strcmp(buffer, "/quit") == 0)
				break;
			clearTextBox();
		}
	}

	terminate_ncurses();
	return 0;
}

