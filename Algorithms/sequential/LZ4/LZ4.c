#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>

#define MAX_MATCH_LENGTH  255
#define MIN_MATCH_LENGTH  4
#define WINDOW_SIZE       65536 //due to the 2 bytes limit for the match offset
#define DEFAULT_LOG_FILE "../../../Output-Input/log/encoding_log.txt"
#define DEFAULT_OUTPUT_BIN_FILE "../../../Output-Input/bin/output.bin"
#define DEFAULT_INPUT_FILE "../../../Output-Input/input/input.txt"

typedef struct {
    uint8_t token;
    uint8_t* literals;
    size_t literal_length;
    uint16_t match_offset;
    size_t match_length;
} LZ4Sequence;

typedef struct {
    uint8_t token;
    size_t sequences;
    LZ4Sequence* block_sequences;
} LZ4Block;

typedef struct {
    uint8_t* input_data;
    size_t input_size;
} LZ4Context;

FILE* log_file;
LZ4Block currentBlock;

void print_binary_to_file(FILE* file, uint8_t num) {
    for (int i = 7; i >= 0; i--) {
        fprintf(file, "%d", (num >> i) & 1);
    }
}

void print_sequence_details(FILE* log_file, LZ4Sequence* sequence) {
    fprintf(log_file, "\n");
    print_binary_to_file(log_file, sequence->token);
    fprintf(log_file, "\nMatch Offset: %u\n", sequence->match_offset);
    fprintf(log_file, "Match Length: %zu\n", sequence->match_length);
    fprintf(log_file, "Literal Length: %zu\n", sequence->literal_length);
    fprintf(log_file, "Literals: ");
    for (size_t i = 0; i < sequence->literal_length; i++) {
        if (sequence->literals[i] >= 32 && sequence->literals[i] <= 126) {
            fprintf(log_file, "%c", sequence->literals[i]);
        } else {
            fprintf(log_file, "0x%02X", sequence->literals[i]);
        }
    }
    fprintf(log_file, "\n");
}

void print_block_details(FILE* log_file, LZ4Block* block) {
    fprintf(log_file, "Block encoded, containing %zu sequences:\n", block->sequences);
    for (size_t i = 0; i < block->sequences; i++) {
        print_sequence_details(log_file, &block->block_sequences[i]);
    }
    fprintf(log_file, "\n");
}

uint8_t find_longest_match(uint8_t* input, size_t current_index, uint8_t* match_distance) {
    size_t longest_match_length = 0;
    size_t longest_match_distance = 0;
    size_t window_start = (current_index >= WINDOW_SIZE) ? current_index - WINDOW_SIZE : 0;

    for (size_t i = window_start; i < current_index; ++i) {
        size_t current_match_length = 0;
        while (current_match_length < MAX_MATCH_LENGTH && (input[i + current_match_length] == input[current_index + current_match_length])) {
            ++current_match_length;
        }
        if (current_match_length > longest_match_length) {
            longest_match_length = current_match_length;
            longest_match_distance = current_index - i;
        }
    }

    if (longest_match_length >= MIN_MATCH_LENGTH) {
        *match_distance = (uint8_t)longest_match_distance;
        return (uint8_t)longest_match_length;
    } else {
        return 0;
    }
}

void write_output(LZ4Sequence sequence, const char* output_file) {//modify to print blocks
    FILE* file = fopen(output_file, "ab");
    fwrite(&sequence.token, sizeof(uint8_t), 1, file);
    if (sequence.literal_length >= 15) {
        uint8_t remaining = sequence.literal_length - 15;
        while (remaining >= 255) {
            uint8_t to_write = 255;
            fwrite(&to_write, sizeof(uint8_t), 1, file);
            remaining -= 255;
        }
        fwrite(&remaining, sizeof(uint8_t), 1, file);
    }
    fwrite(sequence.literals, sizeof(uint8_t), sequence.literal_length, file);
    fwrite(&sequence.match_offset, sizeof(uint16_t), 1, file);
    if (sequence.match_length >= 15) {
        uint8_t remaining = sequence.match_length - 15;
        while (remaining >= 255) {
            uint8_t to_write = 255;
            fwrite(&to_write, sizeof(uint8_t), 1, file);
            remaining -= 255;
        }
        fwrite(&remaining, sizeof(uint8_t), 1, file);
    }
    fclose(file);
}

void add_sequence_to_block(LZ4Sequence seq, LZ4Block* block) {
    if(block->sequences == 0){
        block->block_sequences = malloc(sizeof(LZ4Sequence));
    } else {
        block->block_sequences = realloc(block->block_sequences, sizeof(LZ4Sequence)*(block->sequences+1));
    }
    block->block_sequences[block->sequences] = seq;
    block->sequences += 1;
}

void lz4_encode(LZ4Context* context) {
    size_t input_index = 0;
    uint8_t* input = context->input_data;
    LZ4Sequence sequence;
    uint16_t literal_counter = 0;

    while (input_index < context->input_size) {
        uint8_t match_distance = 0;
        uint8_t match_length = find_longest_match(input, input_index, &match_distance);

        if (match_length < MIN_MATCH_LENGTH) {
            if (literal_counter == 0) {
                sequence.literals = &input[input_index];
            }
            input_index++;
            literal_counter++;
        } else {
            sequence.match_offset = match_distance;
            sequence.literal_length = literal_counter;
            sequence.match_length = match_length;
            uint8_t token_literal_length = (literal_counter >= 15) ? 15 : literal_counter;
            uint8_t token_match_length = (match_length >= 15) ? 15 : match_length - MIN_MATCH_LENGTH;
            sequence.token = (token_literal_length << 4) | token_match_length;
            add_sequence_to_block(sequence, &currentBlock);
            literal_counter = 0;
            input_index += match_length;
        }
    }

    if (literal_counter > 0) {
        sequence.match_offset = 0;
        sequence.literal_length = literal_counter;
        sequence.match_length = 0;
        uint8_t token_literal_length = (literal_counter >= 15) ? 15 : literal_counter;
        sequence.token = (token_literal_length << 4);
        add_sequence_to_block(sequence, &currentBlock);
        //print_sequence_details(log_file, &sequence);
    }
    print_block_details(log_file, &currentBlock);
}

void ensure_directories() {
    if (_mkdir("../../../Output-Input") != 0) {
        perror("Unable to create directory");
    }
    if (_mkdir("../../../Output-Input/bin") != 0) {
        perror("Unable to create directory");
    }
    if (_mkdir("../../../Output-Input/log") != 0) {
        perror("Unable to create directory");
    }
    if (_mkdir("../../../Output-Input/input") != 0) {
        perror("Unable to create directory");
    } else {
        FILE* file = fopen(DEFAULT_INPUT_FILE, "w");
        fprintf(file, "abcabcabc\nline 2\nline 3\n");
        fclose(file);
    }
}

void free_block_sequences(LZ4Block* block) {
    if (block->block_sequences != NULL) {
        for (size_t i = 0; i < block->sequences; ++i) {
            if (block->block_sequences[i].literals != NULL) {
                free(block->block_sequences[i].literals);
            }
        }
        free(block->block_sequences);
        block->block_sequences = NULL;
        block->sequences = 0;
    }
}

int main() {
    ensure_directories();

    FILE* output_file = fopen(DEFAULT_OUTPUT_BIN_FILE, "wb");
    fclose(output_file);
    FILE* log_file_initial = fopen(DEFAULT_LOG_FILE, "w");
    fclose(log_file_initial);

    log_file = fopen(DEFAULT_LOG_FILE, "a");
    FILE* input_file = fopen(DEFAULT_INPUT_FILE, "r");
    if (input_file == NULL) {
        perror("Error: input file doesn't exist");
        return 1;
    }

    LZ4Context context;
    fseek(input_file, 0, SEEK_END);
    long file_size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);

    context.input_data = malloc(file_size + 1);
    if (context.input_data == NULL) {
        perror("Error: Unable to allocate memory");
        fclose(input_file);
        return 1;
    }

    size_t bytes_read = fread(context.input_data, 1, file_size, input_file);
    if (bytes_read != file_size) {
        printf("Error: Failed to read the entire file, current implementation doesn't support line breaks");
        free(context.input_data);
        fclose(input_file);
        return 1;
    }

    context.input_data[file_size] = '\0';

    fclose(input_file);

    printf("File Content:\n%s\n", context.input_data);

    context.input_size = strlen((char*)context.input_data);


    lz4_encode(&context);

    printf("Encoding completed. Check %s for details.\n", DEFAULT_LOG_FILE);

    fclose(log_file);
    free(context.input_data);
    free_block_sequences(&currentBlock);

    return 0;
}
