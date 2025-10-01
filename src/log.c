#include <stdarg.h>
#include <time.h>
#include "log.h"

static FILE *logfile = NULL;

int log_init(const char *path) {
    logfile = fopen(path, "a");
    if (!logfile) return -1;

    // opcional: escritura sin buffer para debug inmediato
    setvbuf(logfile, NULL, _IONBF, 0);

    return 0;
}

void log_close(void) {
    if (logfile) {
        fclose(logfile);
        logfile = NULL;
    }
}

void log_text(const char *fmt, ...) {
    va_list args1, args2;
    va_start(args1, fmt);
    va_copy(args2, args1);

    // timestamp
    time_t now = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // consola
    printf("[%s] ", ts);
    vprintf(fmt, args1);
    printf("\n");

    // archivo
    if (logfile) {
        fprintf(logfile, "[%s] ", ts);
        vfprintf(logfile, fmt, args2);
        fprintf(logfile, "\n");
        fflush(logfile);
    }

    va_end(args1);
    va_end(args2);
}
