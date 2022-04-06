#pragma once
#include <string>

void RequireNotNull(void *ptr, const std::string &message);
void RequireZero(int rc, const std::string &message);
