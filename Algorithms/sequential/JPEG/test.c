#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    int count;
    int value;
} frequency;

typedef struct Node {
    int count;
    int value;
    struct Node* left;
    struct Node* right;
} Node;

typedef struct {
    int value;
    char code[32];
} HuffmanCode;

void calculate_frequency(frequency* frequencies, size_t input_len, int* input, size_t* unique_count) {
    for (size_t i = 0; i < input_len; i++) {
        int found = 0;
        for (size_t j = 0; j < (*unique_count); j++) {
            if (frequencies[j].value == input[i]) {
                frequencies[j].count++;
                found = 1;
                break;
            }
        }
        if (!found) {
            frequencies[(*unique_count)].value = input[i];
            frequencies[(*unique_count)].count = 1;
            (*unique_count)++;
        }
    }
}

void swap(Node* a, Node* b) {
    Node temp = *a;
    *a = *b;
    *b = temp;
}

void heapify(Node* heap, size_t heap_size, size_t i) {
    size_t smallest = i;
    size_t left = 2 * i + 1;
    size_t right = 2 * i + 2;

    if (left < heap_size && heap[left].count < heap[smallest].count)
        smallest = left;

    if (right < heap_size && heap[right].count < heap[smallest].count)
        smallest = right;

    if (smallest != i) {
        swap(&heap[i], &heap[smallest]);
        heapify(heap, heap_size, smallest);
    }
}

void build_heap(frequency* frequencies, size_t unique_count, Node** heap) {
    *heap = malloc(sizeof(Node) * unique_count);
    if (!*heap) {
        printf("Memory allocation failed for heap\n");
        return;
    }

    for (size_t i = 0; i < unique_count; i++) {
        (*heap)[i].count = frequencies[i].count;
        (*heap)[i].value = frequencies[i].value;
        (*heap)[i].left = NULL;
        (*heap)[i].right = NULL;
    }

    for (size_t i = (unique_count / 2) - 1; i < unique_count; i--) {
        heapify(*heap, unique_count, i);
    }
}

Node* build_huffman_tree(Node* heap, size_t* heap_size) {
    while (*heap_size > 1) {
        Node left = heap[0];
        heap[0] = heap[--(*heap_size)];
        heapify(heap, *heap_size, 0);

        Node right = heap[0];
        heap[0] = heap[--(*heap_size)];
        heapify(heap, *heap_size, 0);

        Node* new_node = malloc(sizeof(Node));
        new_node->count = left.count + right.count;
        new_node->value = -1;
        new_node->left = malloc(sizeof(Node));
        *new_node->left = left;
        new_node->right = malloc(sizeof(Node));
        *new_node->right = right;

        heap[*heap_size] = *new_node;
        (*heap_size)++;
        heapify(heap, *heap_size, (*heap_size) - 1);
    }
    return &heap[0];
}

void assign_codes(Node* root, char* code, int depth, HuffmanCode* codes, int* code_index) {
    if (!root) return;

    if (root->value != -1) {
        codes[*code_index].value = root->value;
        code[depth] = '\0';
        strcpy(codes[*code_index].code, code);
        (*code_index)++;
        return;
    }

    code[depth] = '0';
    assign_codes(root->left, code, depth + 1, codes, code_index);

    code[depth] = '1';
    assign_codes(root->right, code, depth + 1, codes, code_index);
}

void generate_encoded_sequence(int* input, size_t input_len, HuffmanCode* codes, int code_count, char* encoded_sequence) {
    encoded_sequence[0] = '\0';
    for (size_t i = 0; i < input_len; i++) {
        for (int j = 0; j < code_count; j++) {
            if (codes[j].value == input[i]) {
                strcat(encoded_sequence, codes[j].code);
                break;
            }
        }
    }
}

double* decode_huffman(Node* root, const char* encoded_sequence, size_t* decoded_len) {
    Node* current_node = root;
    size_t capacity = 100;
    double* decoded_output = malloc(sizeof(double) * capacity);
    size_t count = 0;

    for (size_t i = 0; encoded_sequence[i] != '\0'; i++) {
        current_node = (encoded_sequence[i] == '0') ? current_node->left : current_node->right;

        if (!current_node->left && !current_node->right) {
            if (count == capacity) {
                capacity *= 2;
                decoded_output = realloc(decoded_output, sizeof(double) * capacity);
            }
            decoded_output[count++] = (double)current_node->value;
            current_node = root;
        }
    }

    *decoded_len = count;
    return decoded_output;
}

HuffmanCode* encode_huffman(int* input, size_t input_len, size_t* code_count, Node** root_out) {
    frequency* frequencies = malloc(sizeof(frequency) * input_len);
    size_t unique_count = 0;
    calculate_frequency(frequencies, input_len, input, &unique_count);

    Node* heap = NULL;
    build_heap(frequencies, unique_count, &heap);

    size_t heap_size = unique_count;
    Node* root = build_huffman_tree(heap, &heap_size);
    *root_out = root;

    HuffmanCode* codes = malloc(sizeof(HuffmanCode) * unique_count);
    int code_index = 0;
    char code[32];
    assign_codes(root, code, 0, codes, &code_index);
    *code_count = code_index;

    free(frequencies);
    free(heap);

    return codes;
}

int main() {
    int input[] = {1, 1, 1, 2, 2, 3, 4};
    size_t input_len = sizeof(input) / sizeof(input[0]);
    size_t code_count;
    Node* root;

    HuffmanCode* codes = encode_huffman(input, input_len, &code_count, &root);

    char encoded_sequence[1024];
    generate_encoded_sequence(input, input_len, codes, code_count, encoded_sequence);
    printf("Encoded Sequence: %s\n", encoded_sequence);

    size_t decoded_len;
    double* decoded_output = decode_huffman(root, encoded_sequence, &decoded_len);

    printf("Decoded Output: ");
    for (size_t i = 0; i < decoded_len; i++) {
        printf("%.2f ", decoded_output[i]);
    }
    printf("\n");

    free(codes);
    free(decoded_output);

    return 0;
}
