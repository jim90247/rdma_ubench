#pragma once
#include <memory>
#include <string>
#include <vector>

#include "ib.h"

struct ServerContext {
  IbContext &ib_ctx_;
  volatile char *const buf_;
  const size_t buf_size_;
  std::vector<std::shared_ptr<QpContext>> req_qps_;
  std::vector<std::shared_ptr<QpContext>> rep_qps_;

  ServerContext(IbContext &ib_ctx);
  ~ServerContext();
  void Init(unsigned int server_threads, unsigned int client_threads,
            std::string ip, unsigned int port);

 private:
  void *zmq_ctx_;
  void *sock_;
};
