// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <stdio.h>
#include <stdint.h>

extern void* mempool_create();
extern void mempool_del(void* map, uint32_t k);
extern void* mempool_get(void* map, uint32_t k);
extern void mempool_destroy(void* map);

int main() {
    void* mempool = mempool_create();
    void *p1, *p2;
    p1 = mempool_get(mempool, 12345);
    *(int*)p1 = 54321;
    p2 = mempool_get(mempool, 12345);
    printf("%d\n", *(int*)p2);
    return 0;
}
