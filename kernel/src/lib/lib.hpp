#pragma once

#include <cstddef>

bool match_wildcard(const char* pattern, const char* str);

extern "C" char* strtok(char* str, const char* delimiters);