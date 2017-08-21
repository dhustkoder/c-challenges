#include <string.h>
#include <unistd.h>
#include <stdarg.h>


static void myprintf(const char* const fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	const char* pw = fmt;
	const char* p = fmt;
	for (; *p != '\0'; ++p) {
		if (*p != '%' || *(p + 1) == '%' || *(p + 1) == '\0')
			continue;

		write(STDOUT_FILENO, pw, p - pw);
		pw = p + 2;
		++p;

		switch (*p) {
		case 'd': {
			char buffer[128];
			int buffidx = 128;
			for (int num = va_arg(args, int); num > 0; num /= 10)
				buffer[--buffidx] = "0123456789"[num % 10];
			write(STDOUT_FILENO, &buffer[buffidx], 128 - buffidx);
			break; 
		        }
		case 's': {
			const char* const str = va_arg(args, const char*);
			write(STDOUT_FILENO, str, strlen(str));
			break;
			}
		}
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
