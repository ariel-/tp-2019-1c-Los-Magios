
#ifndef Console_h__
#define Console_h__

#include <stdatomic.h>
#include <vector.h>

typedef void CLICommandHandlerFn(Vector const* args);

typedef struct
{
    char const* CmdName;
    CLICommandHandlerFn* Handler;
} CLICommand;

typedef void CommandParserFn(char const*);

extern CLICommand const CLICommands[];
extern char CLIPrompt[];
extern atomic_bool ProcessRunning;
extern CommandParserFn* CommandParser;

void SigintSetup(void);
void* CliThread(void*);

#endif //Console_h__
