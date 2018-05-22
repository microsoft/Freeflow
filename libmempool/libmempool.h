#include <unordered_map>
#include <queue>
#include <cstdint>
//#include <boost/thread/shared_mutex.hpp> // apt-get install libboost-all-dev
#include <iostream>
#include <pthread.h>
#include "MemoryPool.h"

struct PlaceHolder {
	char data[32];
};

/* Generic Map */
typedef std::unordered_map<uint32_t, struct PlaceHolder*> Map;

class MemPool
{
public:
	MemPool();
	~MemPool();

	void* insert(uint32_t key);
	void* get(uint32_t key);
	void del(uint32_t key);

private:
	Map m_map;
	MemoryPool<struct PlaceHolder> m_pool;
	pthread_spinlock_t m_lock;
};

class MemQueue
{
public:
	MemQueue();
	~MemQueue();

	void* get_tail();
	void del_tail();
	void* get_new_head();

private:
	std::queue<struct PlaceHolder*> m_queue;
	MemoryPool<struct PlaceHolder> m_pool;	
	pthread_spinlock_t m_lock;
};

/* C wrapper compiler */
extern "C" {
	void* mempool_create() {
		return reinterpret_cast<void*> (new MemPool);
	}

	void mempool_del(void* p, uint32_t k) {
		MemPool* m = reinterpret_cast<MemPool*> (p);
		m->del(k);
	}

	void* mempool_insert(void* p, uint32_t k) {
		MemPool* m = reinterpret_cast<MemPool*> (p);
		return m->insert(k);
	}

	void* mempool_get(void* p, uint32_t k) {
		MemPool* m = reinterpret_cast<MemPool*> (p);
		return m->get(k);
	}

	void mempool_destroy(void* p) {
		MemPool* m = reinterpret_cast<MemPool*> (p);
		delete m;
	}

	void* memqueue_create() {
		return reinterpret_cast<void*> (new MemQueue);
	}

	void* memqueue_get_new_head(void* p) {
		MemQueue* m = reinterpret_cast<MemQueue*> (p);
		return m->get_new_head();
	}

	void* memqueue_get_tail(void* p) {
		MemQueue* m = reinterpret_cast<MemQueue*> (p);
		return m->get_tail();
	}

	void memqueue_del_tail(void* p) {
		MemQueue* m = reinterpret_cast<MemQueue*> (p);
		m->del_tail();
	}

	void memqueue_destroy(void* p) {
		MemQueue* m = reinterpret_cast<MemQueue*> (p);
		delete m;
	}

} /* extern "C" */

