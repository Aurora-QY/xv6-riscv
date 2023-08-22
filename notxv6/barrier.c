#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

static int nthread = 1;
static int round = 0;

struct barrier {
  pthread_mutex_t barrier_mutex;//互斥锁，用于保护对 barrier 结构体中的数据的并发访问。在访问 barrier 结构体中的变量时，需要先获取该锁，以确保多个线程不会同时修改这些变量。
  pthread_cond_t barrier_cond;//条件变量，用于实现线程在达到屏障点时的等待和唤醒操作。当一个线程达到屏障点时，它会等待在这个条件变量上，直到所有参与的线程都达到了屏障点，然后通过调用pthread_cond_broadcast 唤醒所有等待在该条件变量上的线程。
  int nthread;      // Number of threads that have reached this round of the barrier //记录达到当前屏障点的线程数量。每个线程达到屏障点时会增加这个计数器，当计数器达到预期值时，表示所有线程都已经到达，可以执行下一轮的屏障操作。
  int round;     // Barrier round //记录当前屏障的轮数。每次所有线程都到达屏障点后，会增加这个计数器，表示进入了下一轮的屏障操作。
} bstate;

static void
barrier_init(void)
{
  assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);
  assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);
  bstate.nthread = 0;
}

static void 
barrier()
{
  // YOUR CODE HERE
  //
  // Block until all threads have called barrier() and
  // then increment bstate.round.
  //
  pthread_mutex_lock(&bstate.barrier_mutex);
  if(++bstate.nthread == nthread) {//检查当前已经调用barrier()的线程数量是否达到了总线程数nthread。如果是最后一个线程到达，就意味着所有线程都已经到达屏障点。
    bstate.nthread = 0;					// 如果是最后一个线程到达，则将bstate.nthread重置为0
    bstate.round++;					// 将bstate.round（表示轮数）递增
    pthread_cond_broadcast(&bstate.barrier_cond);	// 并使用pthread_cond_broadcast函数广播信号给所有等待在bstate.barrier_cond条件变量上的线程，以唤醒它们。
  } else {
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex); // 如果不是最后一个线程到达，则当前线程通过pthread_cond_wait函数在bstate.barrier_cond条件变量上等待，同时释放之前获取的互斥锁。这将使得线程暂停执行，直到被唤醒。
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);
}

static void *
thread(void *xa)
{
  long n = (long) xa;
  long delay;
  int i;

  for (i = 0; i < 20000; i++) {
    int t = bstate.round;
    assert (i == t);
    barrier();
    usleep(random() % 100);
  }

  return 0;
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  long i;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "%s: %s nthread\n", argv[0], argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);

  barrier_init();

  for(i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, thread, (void *) i) == 0);
  }
  for(i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  printf("OK; passed\n");
}
