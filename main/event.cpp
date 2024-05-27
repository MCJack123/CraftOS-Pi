#include <circle/sched/mutex.h>
#include <circle/sched/semaphore.h>
#include <stddef.h>
#include "event.hpp"

struct queue_t {
    event_t value;
    queue_t * next;
};

static queue_t * volatile queue_head = NULL, * volatile queue_tail = NULL;
static CMutex queue_lock;
static CSemaphore queue_sem;

void event_push(const event_t* event) {
    //queue_lock.Acquire();
    queue_t * q = new queue_t;
    q->value = *event;
    q->next = NULL;
    if (queue_tail) queue_tail->next = q;
    queue_tail = q;
    if (!queue_head) queue_head = q;
    //queue_lock.Release();
    queue_sem.Up();
}

void event_wait(event_t* event) {
    do {queue_sem.Down();}
    while (queue_head == NULL);
    //queue_lock.Acquire();
    //while (queue_head == NULL) {queue_tail = NULL;}
    *event = queue_head->value;
    queue_t * q = queue_head->next;
    delete queue_head;
    queue_head = q;
    if (!queue_head) queue_tail = NULL;
    //queue_lock.Release();
}

void event_flush(void) {
    //queue_lock.Acquire();
    queue_t * q;
    while ((q = queue_head)) {
        queue_head = q->next;
        delete q;
    }
    queue_sem = CSemaphore(); // deadlock? don't call when waiting for an event!!!
    //queue_lock.Release();
}
