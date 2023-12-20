#pragma once
#include <string>

// Logging
#define DISABLE_LOGGING

#ifndef DISABLE_LOGGING
#define LOG(Category, Verbosity, Format, ...) std::cout << Category << ": " << Verbosity << ": " << std::format(Format, __VA_ARGS__) << std::endl
#define LOG_ALWAYS(Category, Verbosity, Format, ...) LOG(Category, Verbosity, Format, __VA_ARGS__)
#else
#define LOG(Category, Verbosity, Format, ...) //
#define LOG_ALWAYS(Category, Verbosity, Format, ...) std::cout << Category << ": " << Verbosity << ": " << std::format(Format, __VA_ARGS__) << std::endl
#endif

// Loggin categories
const std::string LogTemp = "LogTemp";
const std::string LogMasterControl = "LogMasterControl";
const std::string LogWorker = "LogWorker";

const std::string Error = "Error";
const std::string Warning = "Warning";
const std::string Info = "Info";
const std::string Debug = "Debug";