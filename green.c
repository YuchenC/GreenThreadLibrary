#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <assert.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

#include "green.h"

#define TRUE 1
#define FALSE 0
#define STACK_SIZE 4096
#define PERIOD 100

static ucontext_t main_cntx = {0};
static green_t main_green = {&main_cntx, NULL, NULL, NULL, NULL, FALSE};
static green_t *running = &main_green;
static sigset_t block;

static void init() __attribute__((constructor));
void timer_handler(int);

void enqueue(struct Queue *q, struct green_t *new){
  //printf("enqueue\n");
  if(q->first == NULL) {
    q->first = new;
  }
  else
    q->last->next = new;
  q->last = new;
}

// Function to remove a key from given queue, readyQ
struct green_t *dequeue(struct Queue *q){
  //printf("dequeue\n");
  // If queue is empty, return NULL.
  if (q->first == NULL) {
    q->last = NULL;
    return NULL;
  }

  // Store previous first and move first one node ahead
  struct green_t *temp = q->first;
  q->first = q->first->next;
  temp->next = NULL;

  // If first becomes NULL, then change last also to NULL
  if (q->first == NULL)
     q->last = NULL;
    // printf(" end dequeue\n");
  return temp;
}

void timer_handler(int sig){

  //usleep(10);
  printf("i time*********************************************************************\n");
  sigprocmask(SIG_BLOCK, &block, NULL);

  if(readyQ->first == NULL) {
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    return;
  }

  green_t *susp = running;
  enqueue(readyQ, susp);
  green_t *next = dequeue(readyQ);
  running = next;
  //printf("innan swap i time\n");
  swapcontext(susp->context, next->context);
  //printf("efter swap i time\n");
  sigprocmask(SIG_UNBLOCK, &block, NULL);

  //if(readyQ->first == NULL)
  //  printf("empty queue\n");
  //printf("efter unblock i time\n");
}

void init() {
  getcontext(&main_cntx);

  sigemptyset(&block);
  sigaddset(&block, SIGVTALRM);

  struct sigaction act = {0};
  struct timeval interval;
  struct itimerval period;

  act.sa_handler = timer_handler;
  assert(sigaction(SIGVTALRM, &act, NULL) == 0);

  interval.tv_sec = 0;
  interval.tv_usec = PERIOD;
  period.it_interval = interval;
  period.it_value = interval;
  setitimer(ITIMER_VIRTUAL, &period, NULL);
}

struct Queue *createQueue(){
  struct Queue *readyQ = (struct Queue*)malloc(sizeof(struct Queue));
  readyQ->first = readyQ->last = NULL;
  return readyQ;
};

void green_thread(){

  green_t *this = running;
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  (*this->fun)(this->arg);

  printf("klar med test**************************************************************************\n");
sigprocmask(SIG_BLOCK, &block, NULL);
  if(this->join != NULL){

    enqueue(readyQ, this->join);
    while(readyQ->last->next != NULL){
      printf("loop\n");
      readyQ->last = readyQ->last->next;

    }
    this->join = NULL;
    //readyQ->last->next = NULL;
  }else{
    printf("No thread to join\n");
  }

  free(this->context->uc_stack.ss_sp);
  free(this->context);
  this->zombie = TRUE;

  //if(readyQ->first == NULL)
  //  enqueue(readyQ, &main_green);

  //sigprocmask(SIG_BLOCK, &block, NULL);
  green_t *next = dequeue(readyQ);
  running=next;
  //printf("set context \n");


  setcontext(next->context);
  //sigprocmask(SIG_UNBLOCK, &block, NULL);


}

int green_create(green_t *new, void *(*fun)(void*), void *arg) {
  sigprocmask(SIG_BLOCK, &block, NULL);
  ucontext_t *cntx = (ucontext_t*)malloc(sizeof(ucontext_t));
  getcontext(cntx);
  void *stack = malloc(STACK_SIZE);
  cntx->uc_stack.ss_sp = stack;
  cntx->uc_stack.ss_size = STACK_SIZE;
  makecontext(cntx, green_thread, 0);

  new->context = cntx;
  new->fun = fun;
  new->arg = arg;
  new->next = NULL;
  new->join = NULL;
  new->zombie = FALSE;

  enqueue(readyQ, new);
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  return 0;
}

int green_yield() {

  sigprocmask(SIG_BLOCK, &block, NULL);

  //printf("i yield\n");

  //printf("i yield efter block\n");
  green_t *susp = running;
  //sigprocmask(SIG_BLOCK, &block, NULL);
  enqueue(readyQ, susp);

  green_t *next = dequeue(readyQ);
  running = next;
  swapcontext(susp->context, next->context);
  sigprocmask(SIG_UNBLOCK, &block, NULL);

  //printf("i yield efter unblock###############\n");
  return 0;
}

int green_join(green_t *thread) {
  sigprocmask(SIG_BLOCK, &block, NULL);
  if(thread->zombie)
    return 0;

  green_t *susp = running;
  if(thread->join == NULL) {
    thread->join = susp;
  } else {
    green_t *temp = thread->join;
    while(temp->next != NULL) {
      temp = temp->next;
      printf("loop2");
    }
    temp->next = susp;
  }

  green_t *next = dequeue(readyQ);
  running = next;

  swapcontext(susp->context, next->context);
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  return 0;
}

void green_cond_init(green_cond_t* cond) {
  sigprocmask(SIG_BLOCK, &block, NULL);
  cond->condQ = createQueue();
  sigprocmask(SIG_UNBLOCK, &block, NULL);
}

void green_cond_wait(green_cond_t* cond) {
  sigprocmask(SIG_BLOCK, &block, NULL);
  green_t *this = running;
  enqueue(cond->condQ, this);
  green_t *next = dequeue(readyQ);
  running = next;
  swapcontext(this->context, next->context);
  sigprocmask(SIG_UNBLOCK, &block, NULL);
}


void green_cond_signal(green_cond_t* cond) {
  sigprocmask(SIG_BLOCK, &block, NULL);
  if(cond->condQ->first != NULL) {
    green_t *signaled = dequeue(cond->condQ);
    enqueue(readyQ, signaled);
  }
  sigprocmask(SIG_UNBLOCK, &block, NULL);
}

void green_cond_broadcast(green_cond_t* cond) {
  sigprocmask(SIG_BLOCK, &block, NULL);
  if(cond->condQ->first != NULL) {
    enqueue(readyQ, cond->condQ->first);
    while(readyQ->last->next != NULL){
      readyQ->last = readyQ->last->next;
    }
    cond->condQ->first = NULL;
    cond->condQ->last = NULL;
  }

  sigprocmask(SIG_UNBLOCK, &block, NULL);
}

//MUTEX
int green_mutex_init(green_mutex_t *mutex) {
  mutex->taken = FALSE;
  mutex->mutexQ = createQueue();
}

int green_mutex_lock(green_mutex_t *mutex) {
  sigprocmask(SIG_BLOCK, &block, NULL);
  green_t *susp = running;
  while(mutex->taken) {
    enqueue(mutex->mutexQ, susp);
    green_t *next = dequeue(readyQ);
    running = next;
    swapcontext(susp->context, next->context);
  }
  mutex->taken = TRUE;
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  return 0;
}

int green_mutex_unlock(green_mutex_t *mutex) {
  sigprocmask(SIG_BLOCK, &block, NULL);

  if(mutex->mutexQ->first != NULL){
    enqueue(readyQ, mutex->mutexQ->first);
    while(readyQ->last->next != NULL){
      readyQ->last = readyQ->last->next;
    }
    mutex->mutexQ->first = NULL;
    mutex->mutexQ->last = NULL;
  }
  mutex->taken = FALSE;
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  return 0;
}

int green_cond_wait2(green_cond_t *cond, green_mutex_t *mutex){
  //printf("in wait2\n");
  sigprocmask(SIG_BLOCK, &block, NULL);
  green_t *this = running;
  enqueue(cond->condQ, this);
  if (mutex != NULL){
    mutex->taken = FALSE;
    green_mutex_unlock(mutex);
  }
  green_t *next = dequeue(readyQ);
  running = next;
  swapcontext(this->context, next->context);
  if (mutex != NULL){
    while(mutex->taken) {
      green_mutex_lock(mutex);
    }
    mutex->taken = TRUE;
  }
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  return 0;
}
int count = 0;
green_mutex_t mutex;

void delay(int i) {
  while(i>0)
    i--;
}

int volatile global = 0;

void *test(void *arg){
  int i = *(int*)arg;
  int loop = 10000;

  green_mutex_init(&mutex);
  while(loop > 0){
    //printf("new while\n");


    count++;
    //printf("thread %d: %d, : %d\n", i, loop, count);
    loop--;

    green_mutex_lock(&mutex);
    int g = global;
    delay(100000);
    //global++;
    global = g+1;
    green_mutex_unlock(&mutex);
    //printf("minskat loop\n");
    //if(i == 0)
      //usleep(1);
    //printf("innan yield\n");
    green_yield();
    //printf("after yield\n");
    //printf("vakanr efter yield thread: %d\n", i);
  }
}

//int count = 0;
int flag = 0;
green_cond_t cond;

void *test2(void *arg) {
    int id = *(int*)arg;
    printf("thread %d is coming!\n", id);
    int loop = 7000;
    while(loop>0) {
        printf("new while, thread: %d\n", id);
        //sigprocmask(SIG_BLOCK, &block, NULL);
      if(flag==id) {
	       //count++;
        //printf("test2 thread %d: %d count: %d\n", id, loop, count);
        global++;
        loop--;
        flag = (id+1)%2;
        printf("flag: %d\n", flag);
        green_cond_signal(&cond);
      }
      else {
        green_cond_wait(&cond);
      }
      //sigprocmask(SIG_UNBLOCK, &block, NULL);
    }
}

int jar = 0;
int max = 20;
green_cond_t condput;
green_cond_t condtake;

void put (int id) {
  green_mutex_lock(&mutex);
  //printf("thread %d is in put\n", id);
  while (jar == max) {
    green_cond_wait2(&condput, &mutex);
  }
  jar++;
  //printf("put in jar = %d\n", jar);
  //printf("PUT(%d) %d\n", id, jar);
  //sleep(1);
  if (jar == max) {
    //printf("jar is full\n");
    green_cond_broadcast(&condtake);
  }
  green_mutex_unlock(&mutex);
}

void take(int id) {
  green_mutex_lock(&mutex);
  //printf("thread %d is in take\n", id);
  while (jar == 0) {
    //printf("thread %d trigger wait2\n", id);
    green_cond_wait2(&condtake, &mutex);
  }
  //printf("jar before = %d\n", jar);
  //printf("TAKE(%d) %d\n", id, jar);
  //sleep(1);
  jar--;
  //printf("jar is emptied %d \n", jar);
  if (jar == 0) {
    green_cond_signal(&condput);
  }
  green_mutex_unlock(&mutex);
}

green_mutex_t lock;


void *test3(void *arg) {
  int id = *(int*)arg;
  //green_mutex_lock(&lock);
  while(1) {
    if (flag == 0) {
      while(1){
        //printf("thread %d\n", id);
        flag = 1;
        put(id);
        //green_mutex_unlock(&lock);
      }
    } else {
      while(1) {
        //printf("thread %d\n", id);
        take(id);
        //delay(100000);
        //green_mutex_unlock(&lock);
        //break;
      }

    }
  }
}

int main(){
  readyQ = createQueue();

  // green_t g0, g1, g2;
  // int a0 = 0;
  // int a1 = 1;
  // int a2 = 2;
  // green_create(&g0, test, &a0);
  // green_create(&g1, test, &a1);
  // //green_create(&g2, test, &a2);
  //
  // //struct green_t *temp = readyQ->first;
  //
  // //sigprocmask(SIG_BLOCK, &block, NULL);
  // green_join(&g0);
  // //sigprocmask(SIG_UNBLOCK, &block, NULL);
  // //printf("new join\n");
  // green_join(&g1);
  // printf("global: %d\n", global);
  //green_join(&g2);

  // printf("############################\n");
  // green_cond_init(&cond);
  //
  // green_t g3, g4; //, g5;
  // int a3 = 0;
  // int a4 = 1;
  // //int a5 = 2;
  // green_create(&g3, test2, &a3);
  // green_create(&g4, test2, &a4);
  // //green_create(&g5, test2, &a5);
  //
  // green_join(&g3);
  // green_join(&g4);
  // //green_join(&g5);

  printf("############################\n");
  green_cond_init(&condput);
  green_cond_init(&condtake);
  green_mutex_init(&mutex);
  green_mutex_init(&lock);

  green_t g5, g6, g7;
  int a5 = 0;
  int a6 = 1;
  int a7 = 2;
  green_create(&g5, test3, &a5);
  green_create(&g6, test3, &a6);
  green_create(&g7, test3, &a7);

  green_join(&g5);
  green_join(&g6);
  green_join(&g7);

  printf("global: %d\n", global);

  return 0;
}
