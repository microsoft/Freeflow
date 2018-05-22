#include "libmempool.h"

MemPool::MemPool() {
	pthread_spin_init(&m_lock, PTHREAD_PROCESS_PRIVATE);
}

MemPool::~MemPool() {
	pthread_spin_destroy(&m_lock);
}

void* MemPool::insert(uint32_t key) {
	pthread_spin_lock(&m_lock);
	PlaceHolder *r = m_pool.allocate();
	m_map[key] = r;
	pthread_spin_unlock(&m_lock);
	return (void*)r;
}

void* MemPool::get(uint32_t key) {
	pthread_spin_lock(&m_lock);
	PlaceHolder *r = m_map[key];
	pthread_spin_unlock(&m_lock);
	return (void*)r;
}

void MemPool::del(uint32_t key) {
	pthread_spin_lock(&m_lock);
	m_pool.deallocate(m_map[key]);
	m_map.erase(key);
	pthread_spin_unlock(&m_lock);
	return;
}

MemQueue::MemQueue() {
	pthread_spin_init(&m_lock, PTHREAD_PROCESS_PRIVATE);
}

MemQueue::~MemQueue() {
	pthread_spin_destroy(&m_lock);
}

void* MemQueue::get_tail() {
	pthread_spin_lock(&m_lock);
	PlaceHolder *r = m_queue.front();
	pthread_spin_unlock(&m_lock);
	return (void*)r;
}

void MemQueue::del_tail() {
	pthread_spin_lock(&m_lock);	
	m_pool.deallocate(m_queue.front());
	m_queue.pop();
	pthread_spin_unlock(&m_lock);
	return;	
}

void* MemQueue::get_new_head() {
	pthread_spin_lock(&m_lock);		
	PlaceHolder *r = m_pool.allocate();
	m_queue.push(r);
	pthread_spin_unlock(&m_lock);
	return (void*)r;
}
