#ifndef MYLIST_H
#define MYLIST_H

#include <stdbool.h>

typedef struct MyListNode MyListNode_t;

struct MyListNode
{
    void *data;
    MyListNode_t *next;
};

typedef struct MyList
{
    MyListNode_t *head;
    MyListNode_t *tail;
} MyList_t;

MyList_t *makeList();
void *listGetFront(MyList_t *list);
void listPopFront(MyList_t *list);
void listInsertFront(void *data, MyList_t *list);
void listInsertEnd(void *data, MyList_t *list);
void listRemove(void *data, MyList_t *list);
void listDisplay(MyList_t *list);
void listDestroy(MyList_t *list);

#endif
