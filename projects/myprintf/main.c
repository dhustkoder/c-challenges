#include <string.h>
#include <unistd.h>
#include <stdarg.h>


static void myprintf(const char* const fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	const int fd = STDOUT_FILENO;

	const char *pw = fmt, *p = fmt;
	while (*p != '\0') {
		if (*p != '%' || *(p + 1) == '%' || *(p + 1) == '\0') {
			++p;
			continue;
		}

		write(fd, pw, p - pw);

		switch (*(p + 1)) {
		case 'd': {
			char buffer[128];
			int buffidx = 128;
			for (int num = va_arg(args, int); num > 0; num /= 10)
				buffer[--buffidx] = "0123456789"[num % 10];
			write(fd, &buffer[buffidx], 128 - buffidx);
			p += 2;
			break; 
		        }
		case 's': {
			const char* const str = va_arg(args, const char*);
			write(fd, str, strlen(str));
			p += 2;
			break;
			}
		}

		pw = p;
	}

	if ((p - pw) > 0)
		write(STDOUT_FILENO, pw, p - pw);

	va_end(args);
}

int main(void)
{

	myprintf("Hello %d World!! %d\n%s\n", 888, 555, "GoodBye World!!");
	return 0;
}
