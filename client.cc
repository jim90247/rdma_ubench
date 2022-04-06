#include "client.h"

#include <spdlog/spdlog.h>
#include <zmq.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "ib.h"
#include "log.h"
#include "zmq_helper.h"

ClientContext::ClientContext(IbContext &ib_ctx)
    : ib_ctx_(ib_ctx), buf_(ib_ctx.buf_), buf_size_(ib_ctx.buf_size_) {
  zmq_ctx_ = zmq_ctx_new();
  RequireNotNull(zmq_ctx_, "zmq_ctx_new failed");
  sock_ = zmq_socket(zmq_ctx_, ZMQ_REQ);
  RequireNotNull(sock_, "zmq_socket failed");
}

ClientContext::~ClientContext() {
  zmq_close(sock_);
  zmq_ctx_destroy(zmq_ctx_);
}

void ClientContext::Init(unsigned int server_threads,
                         unsigned int client_threads, std::string ip,
                         unsigned int port) {
  char url[80];
  BuildZmqUrl(url, 80, ip, port);
  RequireZero(zmq_connect(sock_, url), "zmq_connect failed");

  for (unsigned int i = 0; i < client_threads; i++) {
    auto qp_ctx = std::make_shared<QpContext>(ib_ctx_);
    SendLocalInfo(sock_, *qp_ctx);
    RecvRemoteInfo(sock_, *qp_ctx);
    qp_ctx->Activate();
    req_qps_.push_back(qp_ctx);
  }

  for (unsigned int i = 0; i < server_threads; i++) {
    auto qp_ctx = std::make_shared<QpContext>(ib_ctx_);
    SendLocalInfo(sock_, *qp_ctx);
    RecvRemoteInfo(sock_, *qp_ctx);
    qp_ctx->Activate();
    rep_qps_.push_back(qp_ctx);
  }

  spdlog::info("Client is ready.");
}
