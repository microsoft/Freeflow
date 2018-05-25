
# Freeflow #

Freeflow enables RDMA communication over container overlay network, e.g., Flannel, Weave, etc. The containers do not need direct access to the RDMA NIC hardware. A lightweight Freeflow router handles all the RDMA requests and has full access or QoS control. 

This implementation is a research prototype, and its technical details will be published in academic papers. If you have any questions, please contact the authors below.

Yibo Zhu (yibzh@microsoft.com)

Hongqiang Harry Liu (lampson0505@gmail.com)

Daehyeok Kim (daehyeok@cs.cmu.edu)

Tianlong Yu (tianlony@andrew.cmu.edu)

# Have a try out #

### How to run a demo of Freeflow? ###

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

Step 4: Repeat Step 2 to start customer contaienrs in more hosts.
You can capture a Docker image of node1 for avoiding repeating the installations and building.

Validation: in customer containers, install RDMA perftest tools with "sudo apt-get install perftest". Try "ib_send_bw" or "ib_send_lat".

# Applications #

Freeflow has been tested with RDMA-based Spark (http://hibd.cse.ohio-state.edu/), HERD (https://github.com/efficient/HERD), Tensorflow with RDMA enabled (https://github.com/tensorflow/tensorflow) and rsocket (https://linux.die.net/man/7/rsocket). Most RDMA applications should run with no (or very little) modification, and outperform traditional TCP socket-based implementation.

# Note #

Current released version provides the best isolation between different containers and works the best in multi-tenant environment. This comes with a little performance penalty. We have another version that has the same performance as bare-metal RDMA, while gives up a little isolation. We will release it soon.

