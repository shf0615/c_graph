#include <stdlib.h>
#include <pthread.h>

static pthread_mutex_t lock_a = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lock_b = PTHREAD_MUTEX_INITIALIZER;
int shared_counter = 0;

int complex_func(int x, int y) {
    if (x > 0) {
        for (int i = 0; i < y; i++) {
            if (i % 2 == 0) {
                x += i;
            } else if (i % 3 == 0) {
                x -= i;
            }
        }
    } else if (y > 0) {
        while (x < 100) {
            x = x > 50 ? x + 1 : x + 2;
        }
    }
    return x && y ? x : y;
}

void *worker_a(void *arg) {
    pthread_mutex_lock(&lock_a);
    pthread_mutex_lock(&lock_b);
    shared_counter++;
    pthread_mutex_unlock(&lock_b);
    pthread_mutex_unlock(&lock_a);
    return NULL;
}

void *worker_b(void *arg) {
    pthread_mutex_lock(&lock_b);
    pthread_mutex_lock(&lock_a);
    shared_counter--;
    pthread_mutex_unlock(&lock_a);
    pthread_mutex_unlock(&lock_b);
    return NULL;
}

void leaky_func(void) {
    char *buf = malloc(1024);
    buf[0] = 'x';
    /* no free! */
}

void safe_func(void) {
    char *buf = malloc(256);
    buf[0] = 'y';
    free(buf);
}

int main(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, worker_a, NULL);
    pthread_create(&t2, NULL, worker_b, NULL);
    complex_func(10, 20);
    leaky_func();
    safe_func();
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    return 0;
}
