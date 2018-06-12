cp libfsocket.so /freeflow/libfsocket.so
while true; do 
	DISABLE_RDMA=1 /router
done
