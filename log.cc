#include "log.h"

#include <spdlog/spdlog.h>

#include <cstdlib>

void RequireNotNull(void *ptr, const std::string &message) {
  if (ptr == nullptr) {
    spdlog::critical(message);
    std::exit(EXIT_FAILURE);
  }
}

void RequireZero(int rc, const std::string &message) {
  if (rc != 0) {
    spdlog::critical(message);
    std::exit(EXIT_FAILURE);
  }
}
