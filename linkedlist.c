#include "linkedlist.h"
#include <stdio.h>
/*
    What is a linked list?
    A linked list is a set of dynamically allocated nodes, arranged in
    such a way that each node contains one value and one pointer.
    The pointer always points to the next member of the list.
    If the pointer is NULL, then it is the last node in the list.

    A linked list is held using a local pointer variable which
    points to the first item of the list. If that pointer is also NULL,
    then the list is considered to be empty.
    -------------------------------               ------------------------------              ------------------------------
    |HEAD                         |             \ |              |             |            \ |              |             |
    |                             |-------------- |     DATA     |     NEXT    |--------------|     DATA     |     NEXT    |
    |-----------------------------|             / |              |             |            / |              |             |
    |LENGTH                       |               ------------------------------              ------------------------------
    |COMPARATOR                   |
    |PRINTER                      |
    |DELETER                      |
    -------------------------------                                         
*/

list_t* CreateList(int (*compare)(void*, void*), void (*print)(void*),
                   void (*delete)(void*)) {
    list_t* list = malloc(sizeof(list_t));
    list->comparator = compare;
    list->printer = print;
    list->deleter = delete;
    list->length = 0;
    list->head = NULL;
    return list;
}

void InsertAtHead(list_t* list, void* val_ref) {
    if(list == NULL || val_ref == NULL)
        return;
    if (list->length == 0) list->head = NULL;

    node_t** head = &(list->head);
    node_t* new_node;
    new_node = malloc(sizeof(node_t));

    new_node->data = val_ref;

    new_node->next = *head;

    // moves list head to the new node
    *head = new_node;
    list->length++;
}

void InsertAtTail(list_t* list, void* val_ref) {
    if (list == NULL || val_ref == NULL)
        return;
    if (list->length == 0) {
        InsertAtHead(list, val_ref);
        return;
    }

    node_t* head = list->head;
    node_t* current = head;
    while (current->next != NULL) {
        current = current->next;
    }

    current->next = malloc(sizeof(node_t));
    current->next->data = val_ref;
    current->next->next = NULL;
    list->length++;
}

void InsertInOrder(list_t* list, void* val_ref) {
    if(list == NULL || val_ref == NULL)
        return;
    if (list->length == 0) {
        InsertAtHead(list, val_ref);
        return;
    }

    node_t** head = &(list->head);
    node_t* new_node;
    new_node = malloc(sizeof(node_t));
    new_node->data = val_ref;
    new_node->next = NULL;

    if (list->comparator(new_node->data, (*head)->data) < 0) {
        new_node->next = *head;
        *head = new_node;
    } else if ((*head)->next == NULL) {
        (*head)->next = new_node;
    } else {
        node_t* prev = *head;
        node_t* current = prev->next;
        while (current != NULL) {
            if (list->comparator(new_node->data, current->data) > 0) {
                if (current->next != NULL) {
                    prev = current;
                    current = current->next;
                } else {
                    current->next = new_node;
                    break;
                }
            } else {
                prev->next = new_node;
                new_node->next = current;
                break;
            }
        }
    }
    list->length++;
}

void* RemoveFromHead(list_t* list) {
    node_t** head = &(list->head);
    void* retval = NULL;
    node_t* next_node = NULL;

    if (list->length == 0) {
        return NULL;
    }

    next_node = (*head)->next;
    retval = (*head)->data;
    list->length--;

    node_t* temp = *head;
    *head = next_node;
    //list->deleter(temp->data);
    free(temp);

    return retval;
}

void* RemoveFromTail(list_t* list) {
    if (list->length == 0) {
        return NULL;
    } else if (list->length == 1) {
        return RemoveFromHead(list);
    }

    void* retval = NULL;
    node_t* head = list->head;
    node_t* current = head;

    while (current->next->next != NULL) { 
        current = current->next;
    }

    retval = current->next->data;
    //list->deleter(current->next->data);
    free(current->next);
    current->next = NULL;

    list->length--;

    return retval;
}

/* indexed by 0 */
void* RemoveByIndex(list_t* list, int index) {
    if (list->length <= index) {
        return NULL;
    }

    node_t** head = &(list->head);
    void* retval = NULL;
    node_t* current = *head;
    node_t* prev = NULL;
    int i = 0;

    if (index == 0) {
        retval = (*head)->data;
        
		node_t* temp = *head;
        *head = current->next;
        //list->deleter(temp->data);
        free(temp);
        
		list->length--;
        return retval;
    }

    while (i++ != index) {
        prev = current;
        current = current->next;
    }

    prev->next = current->next;
    retval = current->data;
    //list->deleter(current->data);
    free(current);

    list->length--;

    return retval;
}

void DeleteList(list_t** list) {
    if (list != NULL){
        node_t *del = (*list)->head;
        while (del != NULL){
            node_t *cur = del->next;
            ((*list)->deleter)(del->data);
            free(del);
            del = cur;
        }
        free(*list);
    }
}

void SortList(list_t* list) {
    list_t* new_list = malloc(sizeof(list_t));	
    
	new_list->length = 0;
    new_list->comparator = list->comparator;
    new_list->head = NULL;

    int i = 0;
    int len = list->length;
    for (; i < len; ++i)
    {
        void* val = RemoveFromTail(list);
        InsertInOrder(new_list, val); 
    }

    node_t* temp = list->head;
    list->head = new_list->head;

    new_list->head = temp;
    list->length = new_list->length;  

    DeleteList(&new_list); 
}

void PrintLinkedList(list_t* list) {
    if(list == NULL)
        return;

    node_t* head = list->head;
    while (head != NULL) {
        list->printer(head->data);
        head = head->next;
    }
}
