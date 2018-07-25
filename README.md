
# Freeflow #

Freeflow is a high performance container overlay network that enables RDMA communication and accelerates TCP socket to the same as bare metal. 

Freeflow works on top of popular overlay network solutions including Flannel, Weave, etc. The containers have their individual virtual network interfaces and IP addresses, and do not need direct access to the hardware NIC interface. A lightweight Freeflow library inside containers intercepts RDMA and TCP socket APIs, and a Freeflow router outside containers helps accelerate those APIs. 

Freeflow is developed based on Linux RDMA project (https://github.com/linux-rdma/rdma-core), and released with MIT license.

# Three working modes #

Freeflow works in three modes: fully-isolated RDMA, semi-isolated RDMA, and TCP.

Fully-isolated RDMA provides the best isolation between different containers and works the best in multi-tenant environment. While it offers typical RDMA performance (40Gbps throughput and 1 microsecond latency), this comes with some CPU overhead penalty.

The TCP mode accelerates the TCP socket performance to the same as bare-metal. On a typical Linux server with a 40Gbps NIC, it can achieve 25Gbps throughput for a single TCP connection and less than 20 microsecond latency.

We will release semi-isolated RDMA in the future. It provides the same CPU efficiency as bare-metal RDMA, while does not have full isolation on the data path.

# Performance #

Freeflow TCP costs one more round trip time during connection establishment. The performance (throughput, latency and CPU overhead) afterwards is exactly the same as bare-metal TCP socket.

# Quick Start: run a demo of Freeflow TCP #

Below assumes we use Weave as a base solution of container overlay. Freeflow works with other container overlay as well, e.g., Flannel.

Step 1: Start Freeflow router (one instance per server)

```
sudo docker run -d -it --privileged --net=host -v /freeflow:/freeflow -e "HOST_IP_PREFIX=192.168.1.0/24" --name freeflow freeflow/freeflow:tcp
```

The option "HOST_IP_PREFIX=192.168.1.0/24" means your host subnet is 192.168.1.0/24. Please update it acoording to your own host subnet. 

Step 2: Start iperf container (from an online image without modification)

```
sudo docker run -it --entrypoint /bin/bash --net=weave -v /freeflow:/freeflow -e "VNET_PREFIX=10.32.0.0/12" -e "LD_PRELOAD=/freeflow/libfsocket.so" --name iperf networkstatic/iperf3
```

Step 3: repeat the above steps on another server. Then enter the two iperf3 containers and test. It should show the same performance as bare-metal iperf3.

You can use any other application images to replace iperf3. 

"VNET_PREFIX=10.32.0.0/12" means FreeFlow will treat IP addresses within "10.32.0.0/12" as overlay IP and accelerate connections between two IPs in this range. For IP addresses outside this range, Freeflow will do nothing and use the original network path. 

Freeflow requires both ends are accelerated by Freeflow, or both ends are not accelerated. Mix-and-match will not work. So a good practice is to set VNET_PREFIX the same for all containers and large enough to cover all the IPs you want to accelerate. 

"LD_PRELOAD=/freeflow/libfsocket.so" means all socket calls will be intercepted by FreeFlow. If you want to temporarily disable Freeflow for a specific application, run like this

```
$ LD_PRELOAD="" application_x
```

# Build #

Build the code in libfsocket/ and ffrouter/. Copy the generated binary libfsocket.so and router into docker/ and build docker image from there.

# Applications #

For RDMA, Freeflow has been tested with RDMA-based Spark (http://hibd.cse.ohio-state.edu/), HERD (https://github.com/efficient/HERD), Tensorflow with RDMA enabled (https://github.com/tensorflow/tensorflow) and rsocket (https://linux.die.net/man/7/rsocket). Most RDMA applications should run with no (or very little) modification, and outperform traditional TCP socket-based implementation.

For TCP, Freeflow has also been tested with many applications/framework, including DLWorkspace (https://github.com/Microsoft/DLWorkspace), Horovod (https://github.com/uber/horovod), Memcached, Nginx, PostSQL, and Kafka.

# Contacts #

This implementation is a research prototype that shows the feasibility. It is NOT production quality code. The technical details will be published in academic papers. If you have any questions, please raise issues on Github or contact the authors below.

[Yibo Zhu](http://yibozhu.com) (yibzh@microsoft.com)

[Hongqiang Harry Liu](http://hongqiangliu.com) (hongqiang.liu@alibaba-inc.com)

[Danyang Zhuo](https://danyangzhuo.com/) (danyangz@cs.washington.edu)

