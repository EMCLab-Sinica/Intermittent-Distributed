#include <stdio.h>
#include <stdlib.h>
#include <FreeRTOS.h>
#include "mylist.h"
#include "myuart.h"

MyListNode_t *createNode(void *data);

MyListNode_t *createNode(void *data)
{
    MyListNode_t *newNode = pvPortMalloc(sizeof(MyListNode_t));
    if (!newNode)
    {
        return NULL;
    }
    newNode->data = data;
    newNode->next = NULL;
    return newNode;
}

MyList_t *makeList()
{
    MyList_t *list = pvPortMalloc(sizeof(MyList_t));
    if (!list)
    {
        return NULL;
    }
    list->head = NULL;
    list->tail = NULL;
    return list;
}

void listDisplay(MyList_t *list)
{
    MyListNode_t *current = list->head;
    if (list->head == NULL)
        return;

    for (; current != NULL; current = current->next)
    {
        print2uart("%x\n", current->data);
    }
}

void *listGetFront(MyList_t *list)
{
    if (list->head == NULL)
    {
        return NULL;
    }
    else
    {
        return list->head->data;
    }
}

void listPopFront(MyList_t *list)
{
    MyListNode_t *current = NULL;
    if (list->head == NULL)
    {
        return;
    }
    else
    {
        current = list->head;
        list->head = current->next;
        vPortFree(current->data); // free the data
        vPortFree(current);       // free the node

        return;
    }
}

void listInsertFront(void *data, MyList_t *list)
{
    MyListNode_t *current = NULL;
    if (list->head == NULL)
    {
        list->head = createNode(data);
        list->tail = list->head;
    }
    else
    {
        current = list->head;
        list->head = createNode(data);
        list->head->next = current;
    }
}

void listInsertEnd(void *data, MyList_t *list)
{
    if (list->head == NULL)
    {
        list->head = createNode(data);
        list->tail = list->head;
    }
    else
    {
        list->tail->next = createNode(data);
        list->tail = list->tail->next;
    }
}

void listRemove(void *data, MyList_t *list)
{
    MyListNode_t *current = list->head;
    MyListNode_t *previous = current;
    while (current != NULL)
    {
        if (current->data == data)
        {
            previous->next = current->next;
            if (current == list->head)
                list->head = current->next;
            if (current == list->tail)
                list->tail = previous;
            vPortFree(current->data); // free the data
            vPortFree(current);       // free the node
            return;
        }
        previous = current;
        current = current->next;
    }
}

void listDestroy(MyList_t *list)
{
    MyListNode_t *current = list->head;
    MyListNode_t *next = current;
    while (current != NULL)
    {
        next = current->next;
        vPortFree(current);
        current = next;
    }
    vPortFree(list);
}
