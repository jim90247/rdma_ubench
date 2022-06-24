# RDMA ubench

A multi-threaded microbenchmark to measure the maximum throughput of one-sided RDMA verbs (`RDMA READ` and `RDMA WRITE`) between a server node and a client node.

## Build

### Dependencies

- [rdma-core](https://github.com/linux-rdma/rdma-core): RDMA library. We implement the benchmark with `libibverbs`.
- [libzmq](https://github.com/zeromq/libzmq): exchange queue pair info between the server and the client.
- [gflags](https://github.com/gflags/gflags): parse command-line flags.
- [spdlog](https://github.com/gabime/spdlog): better logging.

Install these packages from your Linux distribution's package manager or compile `libzmq` with CMake to generate the CMake dependency file (except for rdma-core).

### Compile

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

The binary `ubench` will be available in `build/` after the build completes.

## Usage

Server:

```bash
./ubench --server [other options...]
```

Client:

```bash
./ubench [other options...]
```

The client will issue RDMA requests to the server and report the throughput (op/s).

### Options

- `--zmq_ip`: ZeroMQ server IP
- `--zmq_port`: ZeroMQ server port
- `--ib_dev`: RDMA device (e.g. `mlx5_0`)
- `--ib_port`: RDMA device port (default to 1)
- `--server_threads`: Number of server threads. Note that the server is basically idle after the connection has been established. Thus we only create the buffer for each server thread but do not actually launch the threads.
- `--client_threads`: Number of client threads which issue one-sided RDMA verbs to access the server.
- `--msg_size`: RDMA payload size.
- `--batch`: Number of work requests to post to the RDMA device with `ibv_post_send`.
- `--read`: Perform `RDMA READ` instead of `RDMA WRITE`. Run `ubench` without the flag performs `RDMA WRITE`.
- `--sec`: Benchmark duration (seconds).

Default values for these flags can be found in [`ubench.cc`](ubench.cc).
