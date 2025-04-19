/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2024-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* https://github.com/lengjingzju/jcore     *
*******************************************/
#include <stdlib.h>
#include <stdio.h>
#include "jpqueue.h"

typedef struct {
    int id;
    int prio;
    int index;
} Task;

static int jpqueue_cmp(void *a, void *b)
{
    Task *ta = (Task *)a;
    Task *tb = (Task *)b;
    return ta->prio - tb->prio;
}

static void jpqueue_set(void *item, int index)
{
    Task *titem = (Task *)item;
    titem->index = index;
}

static void jpqueue_check(jpqueue_t *pq, int i)
{
    int j = 0;
    Task *item = NULL, *icmp = NULL;

    if (i >= pq->size)
        return;

    item = (Task *)pq->array[i];

    j = 2 * i + 1;
    if (j < pq->size) {
        icmp = (Task *)pq->array[j];
        if (item->prio > icmp->prio) {
            printf("Index: %d, ID: %d, Priority: %d > Index: %d, ID: %d, Priority: %d\n",
                i, item->id, item->prio, j, icmp->id, icmp->prio);
        }
        jpqueue_check(pq, j);
    } else {
        return;
    }

    ++j;
    if (j < pq->size) {
        icmp = (Task *)pq->array[j];
        if (item->prio > icmp->prio) {
            printf("Index: %d, ID: %d, Priority: %d > Index: %d, ID: %d, Priority: %d\n",
                i, item->id, item->prio, j, icmp->id, icmp->prio);
        }
        jpqueue_check(pq, j);
    } else {
        return;
    }
}

static void jpqueue_print(jpqueue_t *pq)
{
    int i = 0;
    Task *item = NULL;

    for (i = 0; i < pq->size; ++i) {
        item = (Task *)pq->array[i];
        printf("%-2d: Index: %2d, ID: %2d, Priority: %2d\n", i, item->index, item->id, item->prio);
    }
    printf("\n");
    jpqueue_check(pq, 0);
}

int main(void)
{
    Task *head_task = NULL;

    Task task1  = { 1 , 3  , 0};
    Task task2  = { 2 , 2  , 0};
    Task task3  = { 3 , 15 , 0};
    Task task4  = { 4 , 5  , 0};
    Task task5  = { 5 , 4  , 0};
    Task task6  = { 6 , 99 , 0};
    Task task7  = { 7 , 1  , 0};
    Task task8  = { 8 , 20 , 0};
    Task task9  = { 9 , 16 , 0};
    Task task10 = { 10, 10 , 0};

    jpqueue_t pq = {0};
    pq.capacity = 10;
    pq.cmp = jpqueue_cmp;
    pq.iset = jpqueue_set;
    jpqueue_init(&pq);

    jpqueue_add(&pq, &task1);
    jpqueue_add(&pq, &task2);
    jpqueue_add(&pq, &task3);
    jpqueue_add(&pq, &task4);
    jpqueue_add(&pq, &task5);
    jpqueue_add(&pq, &task6);
    jpqueue_add(&pq, &task7);
    jpqueue_add(&pq, &task8);
    jpqueue_add(&pq, &task9);
    jpqueue_add(&pq, &task10);

    printf("Priority Queue elements are:\n");
    jpqueue_print(&pq);

    head_task = (Task *)jpqueue_pop(&pq);
    printf("Extracted min is ID: %d, Priority: %d\n", head_task->id, head_task->prio);
    printf("Priority Queue elements are:\n");
    jpqueue_print(&pq);

    jpqueue_del(&pq, &task5);
    printf("Detele Task is ID: %d, Priority: %d\n", task5.id, task5.prio);
    printf("Priority Queue elements are:\n");
    jpqueue_print(&pq);

    head_task = (Task *)jpqueue_pop(&pq);
    printf("Extracted min is ID: %d, Priority: %d\n", head_task->id, head_task->prio);
    printf("Priority Queue elements are:\n");
    jpqueue_print(&pq);

    jpqueue_idel(&pq, task3.index);
    printf("Detele Task is ID: %d, Priority: %d\n", task3.id, task3.prio);
    printf("Priority Queue elements are:\n");
    jpqueue_print(&pq);

    jpqueue_del(&pq, &task9);
    printf("Detele Task is ID: %d, Priority: %d\n", task9.id, task9.prio);
    printf("Priority Queue elements are:\n");
    jpqueue_print(&pq);

    jpqueue_idel(&pq, task6.index);
    printf("Detele Task is ID: %d, Priority: %d\n", task6.id, task6.prio);
    printf("Priority Queue elements are:\n");
    jpqueue_print(&pq);

    jpqueue_uninit(&pq);

    return 0;
}
