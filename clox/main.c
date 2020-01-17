#include <stdio.h>
#include <stdlib.h>

struct node {
    struct node* next;
    struct node* prev;
    int data;
};

struct node* alloc_node(int data) {
    struct node* result = (struct node*) malloc(sizeof(struct node));
    result->next = NULL;
    result->prev = NULL;
    result->data = data;
    return result;
}

struct list {
    struct node* head;
    struct node* tail;
    int length;
};

struct list* alloc_list() {
    struct list* result = (struct list*) malloc(sizeof(struct list));
    result->head = NULL;
    result->tail = NULL;
    result->length = 0;
    return result;
}

void free_list(struct list* list) {
    struct node* current = list->head;
    while(current != NULL) {
        struct node* tmp = current;
        current = current->next;
        free(tmp);
    }
    free(list);
}

void push(int data, struct list* list) {
    struct node* new_head = alloc_node(data);
    new_head->next = list->head;

    if (list->length == 0) {
        list->head = new_head;
        list->tail = new_head;
    } else {
        list->head->prev = new_head;
        list->head = new_head;
    }

    list->length += 1;
}



void print_list(struct list* list) {
    struct node* current = list->head;
    while(current != NULL) {
        printf("%d => ", current->data);
        current = current->next;
    }
    printf("[null]\n");
}

void print_list_reverse(struct list* list) {
    struct node* current = list->tail;
    while(current != NULL) {
        printf("%d => ", current->data);
        current = current->prev;
    }
    printf("[null]\n");
}

int list_index(int i, struct list* list) {
    if (list->length == 0) {
        fprintf(stderr, "Indexing empty list");
        exit(EXIT_FAILURE);
    }

    if (i < 0 || list->length <= i) {
        fprintf(stderr, "List index out of bounds");
        exit(EXIT_FAILURE);
    }


    struct node* current = list->head;
    while(i > 0){
        current = list->head->next;
        i -= 1;
    }
    return current->data;
}


int main(int argc, char* argv[]) {
    printf("Hello, World!\n");
    struct list* l = alloc_list();
    push(4, l);
    push(1, l);
    push(5, l);

    print_list(l);
    print_list_reverse(l);

    printf("l[1] |-> %d\n", list_index(1, l));
    printf("l[2] |-> %d\n", list_index(2, l));

    //printf("l[2] |-> %d\n", list_index(48, l));

    free_list(l);



    printf("%i\n", 5 / atoi(argv[1]));
    return 0;
}