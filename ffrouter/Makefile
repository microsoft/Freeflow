OBJ = main.o ffrouter.o rdma_api.o shared_memory.o verbs_cmd.o
CXXFLAGS = -std=c++11 -O3
router: $(OBJ)
	g++ -o router $(OBJ) -lrdmacm -libverbs -lpthread -lrt 
	#g++ -o router $(OBJ) -libverbs -lpthread -lrt
main.o: main.cpp constant.h ffrouter.h rdma_api.h shared_memory.h log.h
ffrouter.o: ffrouter.cpp
rdma_api.o: rdma_api.cpp
shared_memory.o: shared_memory.cpp
verbs_cmd.o: verbs_cmd.cpp

clean:
	rm router *.o
