#include "bufqueue.h"

#ifdef ENABLE_QUEUE_LOGGING
#include "utils.h"
#include <android/log.h>
#endif  // ENABLE_QUEUE_LOGGING

static int IsQueueEmpty(struct BufferQueue* queue) {
    return atomic_flag_test_and_set(&queue->empty);
}

void InitBufferQueue(struct BufferQueue* queue, int length, void** storage) {
    queue->length = length;
    queue->buffers = storage;
    queue->head = 0;
    atomic_store(&queue->tail, 0);
    atomic_flag_test_and_set(&queue->empty);
}

void BufferQueuePush(struct BufferQueue* queue, void* buffer) {
    // TODO(mburakov): Adjust memory order
#ifdef ENABLE_QUEUE_LOGGING
    LOG(DEBUG, "%s(%p, %p)", __func__, (void*)queue, buffer);
#endif  // ENABLE_QUEUE_LOGGING
    int tail = atomic_load(&queue->tail);
    queue->buffers[tail] = buffer;
    tail = (tail + 1) % queue->length;
    atomic_store(&queue->tail, tail);
    atomic_flag_clear(&queue->empty);
}

void* BufferQueuePop(struct BufferQueue* queue, int blocking) {
    int empty;
    do {
        empty = IsQueueEmpty(queue);
    } while (empty && blocking);
    if (empty) {
#ifdef ENABLE_QUEUE_LOGGING
        LOG(DEBUG, "%s(%p, 0) -> NULL", __func__, (void*)queue);
#endif  // ENABLE_QUEUE_LOGGING
        return NULL;
    }
    void* result = queue->buffers[queue->head];
    queue->head = (queue->head + 1) % queue->length;
    if (atomic_load(&queue->tail) > queue->head) {
        atomic_flag_clear(&queue->empty);
    }
#ifdef ENABLE_QUEUE_LOGGING
    LOG(DEBUG, "%s(%p, %d) -> %p", __func__, (void*)queue, blocking, result);
#endif  // ENABLE_QUEUE_LOGGING
    return result;
}
