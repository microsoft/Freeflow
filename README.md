
# Freeflow #

Freeflow is a high performance container overlay network that enables RDMA communication and accelerates TCP socket to the same as bare metal. 

Freeflow works on top of popular overlay network solutions including Flannel, Weave, etc. The containers have their individual virtual network interfaces and IP addresses, and do not need direct access to the hardware NIC interface. A lightweight Freeflow library inside containers intercepts RDMA and TCP socket APIs, and a Freeflow router outside containers helps accelerate those APIs. 

Freeflow is developed based on Linux RDMA project (https://github.com/linux-rdma/rdma-core), and released with MIT license.

# Three working modes #

Freeflow works in three modes: fully-isolated RDMA, semi-isolated RDMA, and TCP.

Current released version only includes fully-isolated RDMA, which provides the best isolation between different containers and works the best in multi-tenant environment. While it offers typical RDMA performance (40Gbps throughput and 1 microsecond latency), this comes with some CPU overhead penalty.

We will release the other two modes in the future. Semi-isolated RDMA provides the same CPU efficiency as bare-metal RDMA, while does not have full isolation on the data path. The TCP mode accelerates the TCP socket performance to the same as bare-metal. On a typical Linux server with a 40Gbps NIC, it can achieve 25Gbps throughput for a single TCP connection and less than 20 microsecond latency.

# Quick Start: run a demo of Freeflow #

Below is the steps of running Freeflow in fully-isolated RDMA mode.

Step 1: Start Freeflow router (one instance per server)
```
sudo docker run --name router1 --net host -e "FFR_NAME=router1" -e "LD_LIBRARY_PATH=/usr/lib/:/usr/local/lib/:/usr/lib64/" -v /sys/class/:/sys/class/ -v /freeflow:/freeflow -v /dev/:/dev/ --privileged -it ubuntu:14.04 /bin/bash
```

Then log into the router container with
```
sudo docker run exec -it router1 bash
```

Download and install the same version of RDMA libraries and drivers as the host machine.
Currently, Freeflow is developed and tested with "MLNX_OFED_LINUX-4.0-2.0.0.1-ubuntu14.04-x86_64.tgz"
You can download it from http://www.mellanox.com/page/products_dyn?product_family=26.

Then, checkout the code of libraries-router/librdmacm-1.1.0mlnx/.
Build and install the library to /usr/lib/ (which is default).

Finally, checkout the code of ffrouter/.
Build with "build.sh" in the source folder and run "./router router1".

Step 2: Repeat Step 1 to start router in other hosts.
You can capture a Docker image of router1 for avoiding repeating the installations and building.

Step 3: Start a customer container on the same host as router1
```
sudo docker run --name node1 --net weave -e "FFR_NAME=router1" -e "FFR_ID=10" -e "LD_LIBRARY_PATH=/usr/lib" -e --ipc container:router1 -v /sys/class/:/sys/class/ -v /freeflow:/freeflow -v /dev/:/dev/ --privileged --device=/dev/infiniband/uverbs0 --device=/dev/infiniband/rdma_cm -it ubuntu /bin/bash
```

You may use any container overlay solution. In this example, we use Weave (https://github.com/weaveworks/weave).

Environment variable "FFR_NAME=router1" points to the container to the router (router1) on the same host;
"FFR_ID=10" is the ID of the contaienr in FreeFlow. Each container on the same host should have a unique FFR_ID.
We are removing FFR_ID in next version. 

Downloading and install the same version of RDMA libraries and drivers as the host machine.
Currently, Freeflow is developed and tested with "MLNX_OFED_LINUX-4.0-2.0.0.1-ubuntu14.04-x86_64.tgz"
You can download it from http://www.mellanox.com/page/products_dyn?product_family=26.
Then, checkout the code of libraries/ and libmempool/
Build and install the libraries to /usr/lib/ (which is default).

Step 4: Repeat Step 2 to start customer containers in more hosts.
You can capture a Docker image of node1 for avoiding repeating the installations and building.

Attention: the released implementation hard-codes the host IPs and virtual IP to host IP mapping in https://github.com/Microsoft/Freeflow/blob/master/ffrouter/ffrouter.cpp#L215 and https://github.com/Microsoft/Freeflow/blob/master/ffrouter/ffrouter.h#L76. For quick tests, you can edit it according to your environment. Ideally, the router should read it from container overlay controller/zookeeper/etcd.

Validation: in customer containers, install RDMA perftest tools with "sudo apt-get install perftest". Try "ib_send_bw" or "ib_send_lat".

# Applications #

For RDMA, Freeflow has been tested with RDMA-based Spark (http://hibd.cse.ohio-state.edu/), HERD (https://github.com/efficient/HERD), Tensorflow with RDMA enabled (https://github.com/tensorflow/tensorflow) and rsocket (https://linux.die.net/man/7/rsocket). Most RDMA applications should run with no (or very little) modification, and outperform traditional TCP socket-based implementation.

For TCP, Freeflow has also been tested with many applications/framework, including DLWorkspace (https://github.com/Microsoft/DLWorkspace) and Horovod (https://github.com/uber/horovod).

# Contacts #

This implementation is a research prototype that shows the feasibility. It is NOT production quality code. The technical details will be published in academic papers. If you have any questions, please raise issues on Github or contact the authors below.

Yibo Zhu (yibzh@microsoft.com)

Hongqiang Harry Liu (lampson0505@gmail.com)

Daehyeok Kim (daehyeok@cs.cmu.edu)

Tianlong Yu (tianlony@andrew.cmu.edu)
