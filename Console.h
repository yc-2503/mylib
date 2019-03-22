#ifndef CONSOLE_H_
#define CONSOLE_H_
#define _USER 1
#define _SYS  2
#define _DISABLE 0

#define CONSOLE_TYPE _USER

#define UNUSED ((void)0)
#if (CONSOLE_TYPE == _DISABLE)
#define WriteLine(format, ...) 	UNUSED
#define Write(format, ...) 		UNUSED
#define Console_Out() 	UNUSED
#endif
#if (CONSOLE_TYPE == _USER)
#define WriteLine(format, ...) 	_WriteLine(format, ##__VA_ARGS__)
#define Write(format, ...) 		_Write(format, ##__VA_ARGS__)
#define Console_Out() 	_console_out()
#endif
#if (CONSOLE_TYPE == _SYS)
#define WriteLine(format, ...) 	printf(""format"\n", ##__VA_ARGS__)
#define Write(format, ...) 		printf(format, ##__VA_ARGS__)
#define Console_Out() 	UNUSED
#endif
void InitConsole();
void _WriteLine(const char *format, ...);
void _Write(const char *format, ...);
void _console_out();
void DisConsole();
void EnConsole();
#endif