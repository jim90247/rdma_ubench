#include "ib.h"

#include <infiniband/verbs.h>
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <zmq.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>

#include "log.h"

std::string RemoteInfo::ToString() {
  std::stringstream ss;
  ss << "qpn: " << qpn << ", psn: " << psn << ", lid: " << lid
     << ", addr: " << std::hex << addr << std::dec << ", size: " << size
     << ", rkey: " << rkey;
  return ss.str();
}

IbContext::IbContext(const std::string &devname, int port, volatile char *buf,
                     size_t buf_size) {
  GetIbDevice(devname, port);
  RequireNotNull(ibv_ctx_, "Failed to get ib device.");

  pd_ = ibv_alloc_pd(ibv_ctx_);
  RequireNotNull(pd_, "ibv_alloc_pd failed.");

  assert(buf != nullptr);
  assert(buf_size > 0);
  mr_ = ibv_reg_mr(pd_, const_cast<char *>(buf), buf_size,
                   IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                       IBV_ACCESS_REMOTE_WRITE);
  RequireNotNull(mr_, "ibv_reg_mr failed");
  buf_ = buf;
  buf_size_ = buf_size;
}

IbContext::~IbContext() {
  if (mr_ != nullptr) {
    ibv_dereg_mr(mr_);
  }
  if (pd_ != nullptr) {
    ibv_dealloc_pd(pd_);
  }
  if (ibv_ctx_ != nullptr) {
    ibv_close_device(ibv_ctx_);
  }
}

void IbContext::GetIbDevice(const std::string &devname, int port) {
  int num_devices = 0;
  ibv_device **devices = ibv_get_device_list(&num_devices);
  for (int i = 0; i < num_devices; i++) {
    const char *name = ibv_get_device_name(devices[i]);
    if (devname == name) {
      ibv_context *ctx = ibv_open_device(devices[i]);
      ibv_device_attr dev_attr;
      int rc = 0;
      rc = ibv_query_device(ctx, &dev_attr);
      RequireZero(rc, "ibv_query_device failed");

      assert(port >= 1 && port <= dev_attr.phys_port_cnt);
      ibv_port_attr port_attr;
      rc = ibv_query_port(ctx, port, &port_attr);
      RequireZero(rc, "ibv_query_port failed");

      if (port_attr.state != IBV_PORT_ACTIVE) {
        ibv_close_device(ctx);
        spdlog::error("{} (port {}) is not ACTIVE", devname, port);
        ibv_ctx_ = nullptr;
      } else {
        ibv_ctx_ = ctx;
        port_ = port;
        lid_ = port_attr.lid;
        max_rd_atomic_ = dev_attr.max_qp_rd_atom;
      }
      return;
    }
  }

  spdlog::error("{} not found", devname);
  ibv_ctx_ = nullptr;
}

QpContext::QpContext(IbContext &ib_ctx) : ib_ctx_(ib_ctx) {
  cq_ = ibv_create_cq(ib_ctx_.ibv_ctx_, kMaxSendWr + kMaxRecvWr, nullptr,
                      nullptr, 0);
  RequireNotNull(cq_, "ibv_create_cq failed");

  ibv_qp_init_attr qp_init_attr;
  std::memset(&qp_init_attr, 0, sizeof(ibv_qp_init_attr));
  qp_init_attr.send_cq = cq_;
  qp_init_attr.recv_cq = cq_;
  qp_init_attr.cap.max_send_wr = kMaxSendWr;
  qp_init_attr.cap.max_recv_wr = kMaxRecvWr;
  qp_init_attr.cap.max_send_sge = 1;
  qp_init_attr.cap.max_recv_sge = 1;
  // Actual (returned) inline size may be larger than this value.
  // Works on Mellanox ConnectX-6 RNICs, not sure if this also works on other
  // RNICs.
  qp_init_attr.cap.max_inline_data = 64;
  qp_init_attr.qp_type = IBV_QPT_RC;
  qp_ = ibv_create_qp(ib_ctx_.pd_, &qp_init_attr);
  RequireNotNull(qp_, "ibv_create_qp failed");

  static std::mt19937 gen(42);
  std::uniform_int_distribution<uint32_t> dis(0, 1'000'000);
  psn_ = dis(gen);
}

QpContext::~QpContext() {
  if (qp_ != nullptr) {
    ibv_destroy_qp(qp_);
  }
  if (cq_ != nullptr) {
    ibv_destroy_cq(cq_);
  }
}

void QpContext::Activate() {
  ibv_qp_attr attr;
  std::memset(&attr, 0, sizeof(ibv_qp_attr));

  attr.qp_state = IBV_QPS_INIT;
  attr.qp_access_flags = IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
  attr.pkey_index = 0;
  attr.port_num = ib_ctx_.port_;
  int rc = ibv_modify_qp(
      qp_, &attr,
      IBV_QP_STATE | IBV_QP_ACCESS_FLAGS | IBV_QP_PKEY_INDEX | IBV_QP_PORT);
  RequireZero(rc, "ibv_modify_qp to state INIT failed");

  attr.qp_state = IBV_QPS_RTR;
  attr.path_mtu = IBV_MTU_4096;
  attr.rq_psn = remote_.psn;
  attr.dest_qp_num = remote_.qpn;
  attr.ah_attr.dlid = remote_.lid;
  attr.ah_attr.sl = 0;
  attr.ah_attr.src_path_bits = 0;
  attr.ah_attr.is_global = 0;
  attr.ah_attr.port_num = ib_ctx_.port_;
  attr.max_dest_rd_atomic = ib_ctx_.max_rd_atomic_;
  attr.min_rnr_timer = 12;
  rc = ibv_modify_qp(qp_, &attr,
                     IBV_QP_STATE | IBV_QP_PATH_MTU | IBV_QP_RQ_PSN |
                         IBV_QP_DEST_QPN | IBV_QP_AV |
                         IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER);
  RequireZero(rc, "ibv_modify_qp to state RTR failed");

  attr.qp_state = IBV_QPS_RTS;
  attr.timeout = 14;
  attr.retry_cnt = 7;
  attr.rnr_retry = 7;
  attr.sq_psn = psn_;
  attr.max_rd_atomic = ib_ctx_.max_rd_atomic_;
  rc = ibv_modify_qp(qp_, &attr,
                     IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
                         IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN |
                         IBV_QP_MAX_QP_RD_ATOMIC);
  RequireZero(rc, "ibv_modify_qp to state RTS failed");
}

void SendLocalInfo(void *sckt, const QpContext &ctx) {
  RemoteInfo local;
  local.qpn = ctx.qp_->qp_num;
  local.psn = ctx.psn_;
  local.lid = ctx.ib_ctx_.lid_;
  local.addr = reinterpret_cast<uintptr_t>(ctx.ib_ctx_.buf_);
  local.size = ctx.ib_ctx_.buf_size_;
  local.rkey = ctx.ib_ctx_.mr_->rkey;
  zmq_send(sckt, &local, sizeof(RemoteInfo), 0);
}

void RecvRemoteInfo(void *sckt, QpContext &ctx) {
  zmq_recv(sckt, &ctx.remote_, sizeof(RemoteInfo), 0);
}
