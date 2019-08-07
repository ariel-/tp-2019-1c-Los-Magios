
#ifndef Commands_h__
#define Commands_h__

#include <vector.h>

typedef bool ScriptHandlerFn(Vector const* args);

typedef struct
{
    char const* CmdName;
    ScriptHandlerFn* Handler;
} ScriptCommand;

ScriptHandlerFn HandleSelect;
ScriptHandlerFn HandleInsert;
ScriptHandlerFn HandleCreate;
ScriptHandlerFn HandleDescribe;
ScriptHandlerFn HandleDrop;
ScriptHandlerFn HandleJournal;
ScriptHandlerFn HandleAdd;
ScriptHandlerFn HandleRun;
ScriptHandlerFn HandleMetrics;

extern ScriptCommand const ScriptCommands[];

#endif //Commands_h__
