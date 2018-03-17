#include <stdatomic.h>

struct BufferQueue {
  int length;
  void** buffers;
  int head;
  atomic_int tail;
  atomic_flag empty;
};

void InitBufferQueue(struct BufferQueue* queue, int length, void** storage);
void BufferQueuePush(struct BufferQueue* queue, void* buffer);
void* BufferQueuePop(struct BufferQueue* queue, int blocking);
