#ifndef MYLIST_H
#define MYLIST_H

typedef struct MyListNode MyListNode_t;

struct MyListNode
{
    void *data;
    MyListNode_t *next;
};

typedef struct MyList
{
    MyListNode_t *head;
} MyList_t;

MyList_t *makeList();
void listInsert(void *data, MyList_t *list);
void listRemove(void *data, MyList_t *list);
void listDisplay(MyList_t *list);
void listDestroy(MyList_t *list);

#endif
