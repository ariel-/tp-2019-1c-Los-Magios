
#ifndef File_h__
#define File_h__

#include <stdbool.h>
#include <stdint.h>
#include <vector.h>

typedef struct File File;

typedef enum
{
    F_OPEN_READ   = 0x0,
    F_OPEN_WRITE  = 0x1,
    F_OPEN_APPEND = 0x2
} Mode;

typedef void(*FilterFn)(char const* line);

File* file_open(char const* fileName, Mode mode);
bool file_is_open(File const* file);
char* file_readline(File const* file, uint32_t line);
Vector file_getlines(File const* file);
int file_writelines(File const* file, Vector const* lines);
void file_for_each_line(File const* file, FilterFn fn);
void file_close(File* file);

#endif //File_h__
