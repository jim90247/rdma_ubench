#pragma once

#include <cstddef>
#include <cstdio>
#include <string>

inline void BuildZmqUrl(char *s, size_t len, const std::string &ip,
                        unsigned int port) {
  std::snprintf(s, len, "tcp://%s:%u", ip.c_str(), port);
}
