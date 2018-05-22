#! bin/bash

gcc -o router ./router.c -lrdmacm -libverbs -lpthread -lrt -g
