#include<stdio.h>
#include<string.h>
#include<pthread.h>
#include<stdlib.h>
#include<unistd.h>

pthread_t tid[24];

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

    while(i < 64)
    {
        err = pthread_create(&(tid[i]), NULL, &doSomeThing, NULL);
        if (err != 0)
            printf("\ncan't create thread :[%s]", strerror(err));
        else
            printf("\n Thread created successfully\n");

        i++;
    }

    i=0;
    while(i < 64)
    {
        pthread_join(tid[i], NULL);
	i++;
    }
    return 0;
}
