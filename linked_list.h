struct Node { 
    int data; 
    struct Node* next; 
    struct Node* prev; 
};

void push(struct Node** head_ref, int new_data);

void append(struct Node** head_ref, int new_data);

void insertAfter(struct Node* prev_node, int new_data);

void printList(struct Node* node);

//void deleteNode(Node** head_ref, struct Node* del);
