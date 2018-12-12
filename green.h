#include <ucontext.h>

typedef struct green_t {
  ucontext_t *context;
  void *(*fun)(void*);
  void *arg;
  struct green_t *next;
  struct green_t *join;
  int zombie;
} green_t;

struct Queue{
  struct green_t *first, *last;
};

typedef struct green_cond_t {
  struct Queue *condQ;
}green_cond_t;

typedef struct green_mutex_t {
  volatile int taken;
  struct Queue *mutexQ;
} green_mutex_t;

struct Queue *readyQ;

int green_create(green_t *new, void *(*fun)(void*), void *arg);
int green_yield();
int green_join(green_t *thread);
void green_thread();
struct Queue *createQueue();

void green_cond_init(green_cond_t* cond);
void green_cond_wait(green_cond_t* cond);
int green_cond_wait2(green_cond_t* cond, green_mutex_t *mutex);
void green_cond_signal(green_cond_t* cond);

int green_mutex_init(green_mutex_t *mutex);
int green_mutex_lock(green_mutex_t *mutex);
int green_mutex_unlock(green_mutex_t *mutex);
