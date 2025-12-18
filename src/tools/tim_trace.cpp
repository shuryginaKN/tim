#include "tim_trace.h"

#include <cstdio>
#include <cstdlib>


bool tim::vtracef(tim::severity severity,
                 const char *file_name, std::size_t line,
                 const char *function,
                 const char *format, va_list args)
{
#ifdef TIM_DEBUG
    std::fprintf(stderr, "%s %s:%zu %s: ",
                 tim::severity_title(severity),
                 file_name,
                 line,
                 function);
#else
    (void) file_name;
    (void) line;
    (void) function;

    std::fprintf(stderr, "%s: ", tim::severity_title(severity));
#endif

    va_list args_copy;
    va_copy(args_copy, args);
    std::vfprintf(stderr, format, args_copy);
    va_end(args_copy);
    std::fprintf(stderr, "%s", "\n");

    if (severity == tim::severity::Fatal)
        std::abort();

    return severity > tim::severity::Error;
}

bool tim::tracef(tim::severity severity,
                 const char *file_name, std::size_t line,
                 const char *function,
                 const char *format, ...)
{
    va_list args;
    va_start(args, format);
    const bool res = tim::vtracef(severity, file_name, line, function, format, args);
    va_end(args);
    return res;
}
