#ifndef __LOG_H__
#define __LOG_H__

enum log_level { DEBUG = 0, INFO, WARNING, ERROR };

static enum log_level this_log_level = DEBUG;

static const char * log_level_str[] = { "DEBUG" , "INFO", "WARNING", "ERROR" };

#define log_it(fmt, level_str, ...) \
    fprintf(stderr, "%s: " fmt "\n", level_str, ##__VA_ARGS__);

#define log(level, fmt, ...) \
    do { \
        if (level < this_log_level) \
            break; \
        log_it(fmt, log_level_str[level], ##__VA_ARGS__); \
    } while (0)
#endif