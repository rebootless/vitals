#pragma once

enum class LogLevel { Debug, Info, Warn, Error };

// Opens <config dir>/MM-DD-YYYY.log and truncates it. Logging stays off when the file
// can't be created or another running instance holds it.
void log_open();
void log_close();

void log_write(LogLevel level, const char* tag, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));

// One line when a source first reports and whenever it flips between working and failing
void log_source(const char* name, bool ok, const char* error = nullptr);

#define LOG_DEBUG(tag, ...) log_write(LogLevel::Debug, tag, __VA_ARGS__)
#define LOG_INFO(tag, ...)  log_write(LogLevel::Info,  tag, __VA_ARGS__)
#define LOG_WARN(tag, ...)  log_write(LogLevel::Warn,  tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) log_write(LogLevel::Error, tag, __VA_ARGS__)
