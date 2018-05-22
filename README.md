
# Have a try out #

### How to run a demo of cSocket (acceleration of overlay TCP/IP networking for containers)? ###

Step 0: login to Freeflow docker repo (may skip in the future)
```
sudo docker login -u freeflow -p F99SSOOJ=355d2pEUjn9qfLRged0Neve freeflow.azurecr.io
```

Step 1: Start Freeflow router (one instance per server)
```
sudo docker run -d -it --privileged --net=host -v /freeflow:/freeflow -e "HOST_IP_PREFIX=192.168.1.0/24" --name freeflow freeflow.azurecr.io/freeflow:0.16
```
The option "HOST_IP_PREFIX=192.168.1.0/24" means your host subnet is 192.168.1.0/24. 
Please update it acoording to your own host subnet. 
Also, refer to https://github.com/lampson0505/freeflowimpl/tree/master/docker for the docker image freeflow.azurecr.io/freeflow:0.16.

Step 2: Start iperf container (from an online image without modification)
```
sudo docker run -it --entrypoint /bin/bash --net=weave -v /freeflow:/freeflow -e "VNET_PREFIX=10.32.0.0/12" -e "LD_PRELOAD=/freeflow/libfsocket.so" --name iperf networkstatic/iperf3
```
You can use any base container images. Environment variable "VNET_PREFIX=10.32.0.0/12" means FreeFlow will treat
"IP addresses within "10.32.0.0/12" as overlay IP and others as external IP. For external IPs, connections will 
bypass FreeFlow and directly go with NAT. If you do not set this environment variable, FreeFlow will treat all 
addresses as overlay IP addresses. Environment variable "LD_PRELOAD=/freeflow/libfsocket.so" means all socket calls
will be hijacked into FreeFlow. If you want to use legacy TCP/IP stack, simply set "export LD_PRELOAD=".

### How to run a demo of cVerbs (RDMA overlay networking for containers)? ###

Step 0: login to Freeflow docker repo (may skip in the future)
```
sudo docker login -u freeflow -p F99SSOOJ=355d2pEUjn9qfLRged0Neve freeflow.azurecr.io
```

Step 1: Start Freeflow router (one instance per server)
```
sudo docker run --name router1 --net host -e "FFR_NAME=router1" -e "LD_LIBRARY_PATH=/usr/lib/:/usr/local/lib/:/usr/lib64/" -v /sys/class/:/sys/class/ -v /freeflow:/freeflow -v /dev/:/dev/ --privileged -it ubuntu:14.04 /bin/bash
```
If you login the router container with
```
sudo docker run exec -it router1 bash
```
Downloading and install the same version of RDMA libraries and drivers as the host machine.
Currently, cVerbs is developed and tested with "MLNX_OFED_LINUX-4.0-2.0.0.1-ubuntu14.04-x86_64.tgz"
You can download it from http://www.mellanox.com/page/products_dyn?product_family=26.

Then, checkout the code of https://github.com/lampson0505/freeflowimpl/tree/master/libraries-router/librdmacm-1.1.0mlnx.
Build and install the library to /usr/lib/ (which is default).

Finally, checkout the code of https://github.com/lampson0505/freeflowimpl/tree/master/ffrouter/.
build with "build.sh" in the source folder and run "./router router1".

Step 2: Repeat Step 1 to start router in more hosts.
You can capture a Docker image of router1 for avoiding repeating the installations and building.

Step 3: Start a customer container on the same host as router1
```
sudo docker run --name node1 --net weave -e "FFR_NAME=router1" -e "FFR_ID=10" -e "LD_LIBRARY_PATH=/usr/lib" -e --ipc container:router1 -v /sys/class/:/sys/class/ -v /freeflow:/freeflow -v /dev/:/dev/ --privileged --device=/dev/infiniband/uverbs0 --device=/dev/infiniband/rdma_cm -it ubuntu /bin/bash
```
Environment variable "FFR_NAME=router1" points to the container to the router (router1) on the same host;
"FFR_ID=10" is the ID of the contaienr in FreeFlow. Each container on the same host should have a unique FFR_ID.
We are removing FFR_ID in next version. 

Downloading and install the same version of RDMA libraries and drivers as the host machine.
Currently, cVerbs is developed and tested with "MLNX_OFED_LINUX-4.0-2.0.0.1-ubuntu14.04-x86_64.tgz"
You can download it from http://www.mellanox.com/page/products_dyn?product_family=26.
Then, checkout the code of https://github.com/lampson0505/freeflowimpl/tree/master/libraries-router/librdmacm-1.1.0mlnx.
Build and install the library to /usr/lib/ (which is default).

Then, checkout the code of https://github.com/lampson0505/freeflowimpl/tree/master/libraries/librdmacm-1.1.0mlnx.
Build and install the library to /usr/lib/ (which is default).

Install RDMA perftest tools with "sudo apt-get install perftest". Try with "ib_send_bw" or "ib_send_lat".

Step 4: Repeat Step 2 to start customer contaienrs in more hosts.
You can capture a Docker image of node1 for avoiding repeating the installations and building.

# Old README #

### Summary ###

* Quick summary: freeflow router implemented.
* Version: ffrouter:v7, ffclient:nsdi18

```

sudo docker run --name router1 --net host -e "FFR_NAME=router1" -e "LD_LIBRARY_PATH=/usr/lib/:/usr/local/lib/:/usr/lib64/" -v /sys/class/:/sys/class/ -v /freeflow:/freeflow -v /dev/:/dev/ --privileged -it ffrouter:v7 /bin/bash

sudo docker run --name router2 --net host -e "FFR_NAME=router2" -e "LD_LIBRARY_PATH=/usr/lib/:/usr/local/lib/:/usr/lib64/" --ipc host -v /sys/class/:/sys/class/ -v /freeflow:/freeflow -v /dev/:/dev/ --privileged -it ffrouter:v7 /bin/bash

sudo docker run --name sender --net weave -e "FFR_NAME=router1" -e "FFR_ID=10" -e "LD_LIBRARY_PATH=/usr/lib" --ipc container:router1 -v /sys/class/:/sys/class/ -v /freeflow:/freeflow -v /dev/:/dev/ --privileged --device=/dev/infiniband/uverbs0 --device=/dev/infiniband/rdma_cm -it ffclient:nsdi18 /bin/bash

sudo docker run --name receiver --net weave -e "FFR_NAME=router2" -e "FFR_ID=20" -e "LD_LIBRARY_PATH=/usr/lib" --ipc container:router2 -v /sys/class/:/sys/class/ -v /freeflow:/freeflow -v /dev/:/dev/ --privileged --device=/dev/infiniband/uverbs0 --device=/dev/infiniband/rdma_cm -it ffclient:nsdi18 /bin/bash

```
Build and Run two routers
```
cd freeflowimpl/
./build-router.sh
ffrouter/router ${FFR_NAME}
```

Build receiver/sender
```
./build-client.sh
```

Set up memory unlimited on all containers.
```
ulimit -u unlimited 
```

### How to run spark ###
```
wget http://hibd.cse.ohio-state.edu/download/hibd/rdma-spark-0.9.4-bin.tar.gz
wget http://hibd.cse.ohio-state.edu/download/hibd/rdma-hadoop-2.x-1.2.0-bin.tar.gz
tar zxf rdma-spark-0.9.4-bin.tar.gz
tar zxf rdma-hadoop-2.x-1.2.0-bin.tar.gz
cd rdma-spark-0.9.3-bin
export SPARK_HOME=$(pwd)
cd ..
cd rdma-hadoop-2.x-1.1.0
export HADOOP_HOME=$(pwd)
export PATH=${HADOOP_HOME}/bin:${PATH}
export PATH=${SPARK_HOME}/bin:${PATH}
cp ${SPARK_HOME}/conf/metrics.properties.template ${SPARK_HOME}/conf/metrics.properties
cp ${SPARK_HOME}/conf/spark-defaults.conf.template ${SPARK_HOME}/conf/spark-defaults.conf
echo "spark.ib.enabled  false" >> ${SPARK_HOME}/conf/spark-defaults.conf
echo "spark.roce.enabled  true" >> ${SPARK_HOME}/conf/spark-defaults.conf
echo "hadoop.ib.enabled  false" >> ${SPARK_HOME}/conf/spark-defaults.conf
echo "spark.executor.extraLibraryPath ${SPARK_HOME}/lib/native/Linux-amd64-64:${HADOOP_HOME}/lib/native" >> ${SPARK_HOME}/conf/spark-defaults.conf
echo "spark.driver.extraLibraryPath ${SPARK_HOME}/lib/native/Linux-amd64-64:${HADOOP_HOME}/lib/native" >> ${SPARK_HOME}/conf/spark-defaults.conf
echo "spark.broadcast.factory org.apache.spark.broadcast.HttpBroadcastFactory" >>  ${SPARK_HOME}/conf/spark-defaults.conf

On all slaves: export SPARK_LOCAL_IP={LOCAL_IP}
               export SPARK_MASTER_IP={MASTER_IP}
Start Master: ${SPARK_HOME}/sbin/start-master.sh -h ${SPARK_MASTER_IP}
Start Slave: ${SPARK_HOME}/sbin/start-slave.sh -c 1 -m 2G spark://${SPARK_MASTER_IP}:7077
Commit job: ${SPARK_HOME}/bin/spark-submit --class org.apache.spark.examples.GroupByTest --files $SPARK_HOME/conf/metrics.properties --master spark://${SPARK_MASTER_IP}:7077 --driver-memory 1g --executor-memory 1g --executor-cores 1 $SPARK_HOME/examples/jars/spark-examples_2.11-2.1.0.jar 24 16384 1024 24
```

### How to run HERD ###
```
cd /freeflowimpl/rdma_app/herd_ff/

Look at "servers" and common.h and run_server.sh (or run_machine.sh) to make sure #server and #clients are consistent.

Look at "conn.c" to make sure server IPs are correct.

Make sure "make clean" first before "make".

To run server: ./run_server.sh

To run client: ./run_machine.sh 0
```

### How to build TensorFlow of RDMA ###
```
root@99cd39c4c5b9:~/tensorflow# ./configure
.............
You have bazel 0.5.1 installed.
Please specify the location of python. [Default is /usr/bin/python]: /usr/bin/python3
Found possible Python library paths:
  /usr/local/lib/python3.5/dist-packages
  /usr/lib/python3/dist-packages
Please input the desired Python library path to use.  Default is [/usr/local/lib/python3.5/dist-packages]

Using python library path: /usr/local/lib/python3.5/dist-packages
Do you wish to build TensorFlow with MKL support? [y/N] N
No MKL support will be enabled for TensorFlow
Please specify optimization flags to use during compilation when bazel option "--config=opt" is specified [Default is -march=native]:
Do you wish to use jemalloc as the malloc implementation? [Y/n]
jemalloc enabled
Do you wish to build TensorFlow with Google Cloud Platform support? [y/N]
No Google Cloud Platform support will be enabled for TensorFlow
Do you wish to build TensorFlow with Hadoop File System support? [y/N]
No Hadoop File System support will be enabled for TensorFlow
Do you wish to build TensorFlow with the XLA just-in-time compiler (experimental)? [y/N]
No XLA support will be enabled for TensorFlow
Do you wish to build TensorFlow with VERBS support? [y/N] y
VERBS support will be enabled for TensorFlow
Do you wish to build TensorFlow with OpenCL support? [y/N]
No OpenCL support will be enabled for TensorFlow
Do you wish to build TensorFlow with CUDA support? [y/N] y
CUDA support will be enabled for TensorFlow
Do you want to use clang as CUDA compiler? [y/N]
nvcc will be used as CUDA compiler
Please specify the CUDA SDK version you want to use, e.g. 7.0. [Leave empty to default to CUDA 8.0]: 8.0
Please specify the location where CUDA 8.0 toolkit is installed. Refer to README.md for more details. [Default is /usr/local/cuda]:
Please specify which gcc should be used by nvcc as the host compiler. [Default is /usr/bin/gcc]:
Please specify the cuDNN version you want to use. [Leave empty to default to cuDNN 6.0]: 5
Please specify the location where cuDNN 5 library is installed. Refer to README.md for more details. [Default is /usr/local/cuda]:
./configure: line 664: /usr/local/cuda/extras/demo_suite/deviceQuery: No such file or directory
Please specify a list of comma-separated Cuda compute capabilities you want to build with.
You can find the compute capability of your device at: https://developer.nvidia.com/cuda-gpus.
Please note that each additional compute capability significantly increases your build time and binary size.
[Default is: "3.5,5.2"]: 6.1
Do you wish to build TensorFlow with MPI support? [y/N]
MPI support will not be enabled for TensorFlow
Configuration finished
root@99cd39c4c5b9:~/tensorflow# bazel build --config=opt --config=cuda //tensorflow/tools/pip_package:build_pip_package
cp bazel-bin/tensorflow/python/_pywrap_tensorflow_internal.so /usr/local/lib/python3.5/dist-packages/tensorflow/python/
```
### How to run rsocket instead of legacy socket ###
```
To run iperf server: LD_PRELOAD=/usr/lib/rsocket/librspreload.so iperf -s
To run iperf client: LD_PRELOAD=/usr/lib/rsocket/librspreload.so iperf -c <SERVER_IP>

`
