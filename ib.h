#pragma once
#include <infiniband/verbs.h>

#include <cstdint>
#include <string>
#include <thread>

struct RemoteInfo {
  // queue pair
  uint32_t qpn;
  uint32_t psn;
  int lid;

  // memory region
  uint32_t rkey;
  uintptr_t addr;
  size_t size;

  std::string ToString();
};

struct IbContext {
  ibv_context *ibv_ctx_;
  int port_;
  int lid_;
  int max_rd_atomic_;
  ibv_pd *pd_;
  ibv_mr *mr_;
  volatile char *buf_;
  size_t buf_size_;

  IbContext(const std::string &devname, int port, volatile char *buf,
            size_t buf_size);
  ~IbContext();

 private:
  void GetIbDevice(const std::string &devname, int port);
};

struct QpContext {
  IbContext &ib_ctx_;
  ibv_cq *cq_;
  ibv_qp *qp_;
  uint32_t psn_;
  RemoteInfo remote_;

  QpContext(IbContext &ib_ctx);
  ~QpContext();
  void Activate();
};

void SendLocalInfo(void *sckt, const QpContext &ctx);

void RecvRemoteInfo(void *sckt, QpContext &ctx);

constexpr uint32_t kMaxSendWr = 64;
constexpr uint32_t kMaxRecvWr = 64;
