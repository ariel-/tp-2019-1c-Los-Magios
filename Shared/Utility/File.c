
#include "File.h"
#include <libcommons/string.h>
#include <Malloc.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_STRING 255

static void _removeCRLF(char*);

struct File
{
    FILE* _imp;
};

File* file_open(char const* fileName, Mode mode)
{
    // pre: mode is a valid Mode
    // filename is a valid path
    char fmode[2] = "";
    switch (mode)
    {
        case F_OPEN_READ:
            fmode[0] = 'r';
            break;
        case F_OPEN_WRITE:
            fmode[0] = 'w';
            break;
        case F_OPEN_APPEND:
            fmode[0] = 'a';
            break;
    }

    File* res = Malloc(sizeof(File));
    res->_imp = fopen(fileName, fmode);
    return res;
}

bool file_is_open(File const* file)
{
    if (!file->_imp)
        return false;

    return ftell(file->_imp) >= 0;
}

char* file_readline(File const* file, uint32_t line)
{
    char* resBuffer = Malloc(MAX_STRING);
    size_t sz = MAX_STRING;
    uint32_t currLine = 1;
    for (; currLine <= line; (void) getline(&resBuffer, &sz, file->_imp), ++currLine);
    rewind(file->_imp);

    //trim eol
    resBuffer[strcspn(resBuffer, "\r\n")] = '\0';
    return resBuffer;
}

Vector file_getlines(File const* file)
{
    struct stat stat_file;
    fstat(fileno(file->_imp), &stat_file);

    char* buffer = Calloc(stat_file.st_size + 1, 1);
    (void) fread(buffer, 1, stat_file.st_size, file->_imp);

    Vector lines = string_split(buffer, "\n");
    Free(buffer);

    string_iterate_lines(&lines, _removeCRLF);
    return lines;
}

int file_writelines(File const* file, Vector const* lines)
{
    // pre: file opened for write/append
    char* contents = string_new();
    size_t strLen = 0;

    for (size_t i = 0; i < Vector_size(lines); ++i)
    {
        char** const line = Vector_at(lines, i);
        strLen += snprintf(NULL, 0, "%s\n", *line);
        string_append_with_format(&contents, "%s\n", *line);
    }

    int res = fwrite(contents, 1, strLen, file->_imp);
    Free(contents);

    return res;
}

void file_for_each_line(File const* file, FilterFn fn)
{
    char* buf = Malloc(MAX_STRING);
    size_t sz = MAX_STRING;

    while (getline(&buf, &sz, file->_imp) > 0)
        fn(buf);

    Free(buf);
}

void file_close(File* file)
{
    if (file->_imp)
        fclose(file->_imp);
    Free(file);
}

static void _removeCRLF(char* line)
{
    line[strcspn(line, "\r\n")] = '\0';
}
