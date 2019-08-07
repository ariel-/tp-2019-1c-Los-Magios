
#ifndef LogCommon_h__
#define LogCommon_h__

typedef enum
{
    LOG_LEVEL_DISABLED     = 0,
    LOG_LEVEL_TRACE        = 1,
    LOG_LEVEL_DEBUG        = 2,
    LOG_LEVEL_INFO         = 3,
    LOG_LEVEL_WARN         = 4,
    LOG_LEVEL_ERROR        = 5,
    LOG_LEVEL_FATAL        = 6,

    NUM_ENABLED_LOG_LEVELS = 6
} LogLevel;

typedef enum
{
    /* opciones generales */
    APPENDER_FLAGS_NONE             = 0x00,
    // prefijar fecha y hora en cada log (precision milisegundos)
    APPENDER_FLAGS_PREFIX_TIMESTAMP = 0x01,
    // prefijar nivel de log en cada entrada de log (INFO/ERROR/etc.)
    APPENDER_FLAGS_PREFIX_LOGLEVEL  = 0x02,

    /* opciones para AppenderFile (archivos) */
    // agregar fecha y hora de creacion luego del nombre de archivo
    APPENDER_FLAGS_USE_TIMESTAMP    = 0x04,
    // si el archivo existe se genera un backup y se crea uno nuevo (solo si el modo de apertura es "w")
    APPENDER_FLAGS_MAKE_FILE_BACKUP = 0x08
} AppenderFlags;

#endif //LogCommon_h__
