#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <locale.h>
#include <curses.h>


#define BUFFER_SIZE ((int)512)
static char buffer[BUFFER_SIZE] = { '\0' };
static int buffer_idx = 0;
static int cx = 0, cy = 0;


static inline void initialize_ncurses(void)
{
	initscr();
	noecho();
	timeout(0);
	keypad(stdscr, TRUE);
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
	move(cy, cx % getmaxx(stdscr));
	refresh();
}


static inline void clearTextBox(void)
{
	cy = cx = 0;
	buffer_idx = 0;
	clear();
	move(0, 0);
	refresh();
}


static inline bool updateTextBox(void)
{
	const int c = getch();

	switch (c) {
	case 10: // also enter (ascii) [fall]
	case KEY_ENTER: // submit msg
		return true;

	case KEY_LEFT:
		if (cx > 0) {
			--cx;
			cy = cx / getmaxx(stdscr);
			move(cy, cx % getmaxx(stdscr));
		}
		return false;
	case KEY_RIGHT:
		if (cx < buffer_idx) {
			++cx;
			cy = cx / getmaxx(stdscr);
			move(cy, cx % getmaxx(stdscr));
		}
		return false;
	case 127: // also backspace (ascii) [fall]
	case KEY_BACKSPACE:
		if (cx > 0) {
			memmove(&buffer[cx - 1], &buffer[cx], buffer_idx - cx);
			buffer[--buffer_idx] = '\0';
			--cx;
			refreshTextBox();
		}
		return false;
	case KEY_HOME:
		cy = cx = 0;
		move(cy, cx);
		return false;
	case KEY_END:
		if (cx < buffer_idx) {
			cx = buffer_idx;
			cy = cx / getmaxx(stdscr);
			move(cy, cx % getmaxx(stdscr));
			return false;
		}
	}

	if (isascii(c) && buffer_idx < BUFFER_SIZE) {
		if (cx < buffer_idx)
			memmove(&buffer[cx + 1], &buffer[cx], buffer_idx - cx);
		buffer[cx++] = (char) c;
		buffer[++buffer_idx] = '\0';
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

