#include<stdio.h>
#include<string.h>
#include<pthread.h>
#include<stdlib.h>
#include<unistd.h>

#define NTHREADS 64

pthread_t tid[NTHREADS];

void* doSomeThing(void *arg)
{
    unsigned long i = 0;
    pthread_t id = pthread_self();

    for(i=0; i<3000000000;i++);

    return NULL;
}

int main(void)
{
    int i = 0;
    int err;

    for(i=0; i<NTHREADS; i++) {
        err = pthread_create(&(tid[i]), NULL, &doSomeThing, NULL);
        if (err != 0)
            printf("\ncan't create thread :[%s]", strerror(err));
        else
            printf("\n Thread created successfully\n");
    }

    for(i=0; i<NTHREADS; i++) {
        pthread_join(tid[i], NULL);
    }
    return 0;
}
