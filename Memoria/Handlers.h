
#ifndef Memoria_Handlers_h__
#define Memoria_Handlers_h__

#include <OpcodeHandler.h>

OpcodeHandlerFnType HandleHandshakeOpcode;
OpcodeHandlerFnType HandleSelectOpcode;
OpcodeHandlerFnType HandleInsertOpcode;
OpcodeHandlerFnType HandleCreateOpcode;
OpcodeHandlerFnType HandleDescribeOpcode;
OpcodeHandlerFnType HandleDropOpcode;
OpcodeHandlerFnType HandleJournalOpcode;

#endif //Memoria_Handlers_h__
