#include <debug.h>

void print_error(const char *format, ...) {
	#if not defined(NDEBUGGING)
	va_list my_args;
	va_start(my_args, format);
	vfprintf(stderr, format, my_args);
	va_end(my_args);
	#else
	#endif
}