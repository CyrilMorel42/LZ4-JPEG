#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>


#define MAX_MATCH_LENGTH  255
#define MIN_MATCH_LENGTH  4
#define WINDOW_SIZE       65536 //due to the 2 bytes limit for the match offset
#define DEFAULT_BLOCK_LENGTH 64
#define DEFAULT_LOG_FILE "../../../Output-Input/log/encoding_log.txt"
#define DEFAULT_OUTPUT_BIN_FILE "../../../Output-Input/bin/output.bin"
#define DEFAULT_INPUT_FILE "../../../Output-Input/input/input.txt"
#define DEFAULT_OUTPUT_HEX_FILE "../../../Output-Input/bin/output.txt"

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
    size_t blocks;
    LZ4Block* frame_blocks;
} LZ4Frame;

typedef struct {
    uint8_t* input_data;
    size_t input_size;
} LZ4Context;


void print_binary_to_file(FILE* file, uint8_t num) {
    for (int i = 7; i >= 0; i--) {
        fprintf(file, "%d", (num >> i) & 1);
    }
}

void dump_to_hex_file(const char *input_filename, const char *output_filename) {
    // Open the input binary file for reading
    FILE *input_file = fopen(input_filename, "rb");
    if (input_file == NULL) {
        perror("Error opening input file");
        return;
    }

    // Open the output text file for writing
    FILE *output_file = fopen(output_filename, "w");
    if (output_file == NULL) {
        perror("Error opening output file");
        fclose(input_file);
        return;
    }

    uint8_t buffer;
    size_t byte_count = 0; // To keep track of the number of bytes printed for formatting

    // Read the input file byte by byte and write to the output file in hexadecimal format
    while (fread(&buffer, 1, 1, input_file) == 1) {
        // Write each byte in hexadecimal format to the output file
        fprintf(output_file, "%02X ", buffer);

        byte_count++;

        // Optionally add a newline every 16 bytes for readability
        if (byte_count % 16 == 0) { // 16 bytes * 3 chars each (2 hex + space) = 48 chars
            fprintf(output_file, "\n");
        }
    }

    // Close the files
    fclose(input_file);
    fclose(output_file);
}


FILE* safe_open(char* file_name, char* mode) {
    FILE* file = fopen(file_name, mode);
    if (file == NULL) {
        perror("Error: Unable to open file");
        exit(1); // Exit the program if critical file operations fail
    }
    return file;
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
    fprintf(log_file, "Block encoded:\n", block->sequences);
    print_binary_to_file(log_file, block->token);
    fprintf(log_file, "\n");
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

void write_sequence(LZ4Sequence sequence, FILE* file) {
    fwrite(&sequence.token, sizeof(uint8_t), 1, file);

    // Write literal length (encoded using the scheme where lengths >= 15 need special encoding)
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

    // Write match offset
    fwrite(&sequence.match_offset, sizeof(uint16_t), 1, file);

    // Write match length (match length must be >= 4, so subtract 4 before encoding)
    if (sequence.match_length >= 4) {
        uint8_t adjusted_match_length = sequence.match_length - 4;

        if (adjusted_match_length >= 15) {
            uint8_t remaining = adjusted_match_length - 15;
            while (remaining >= 255) {
                uint8_t to_write = 255;
                fwrite(&to_write, sizeof(uint8_t), 1, file);
                remaining -= 255;
            }
            fwrite(&remaining, sizeof(uint8_t), 1, file);
        } else {
            // If match length is less than 15, we can just encode it directly
            fwrite(&adjusted_match_length, sizeof(uint8_t), 1, file);
        }
    } else {
        // Error handling or adjusting the match length as needed
        // Match length should always be >= 4 in a valid sequence, so if it's < 4, it's invalid.
        // You may want to handle this with an error or special case handling.
        // For now, we assume it's always >= 4.
    }
}



void write_output(LZ4Block* block, FILE* output_file) {

    // Write the block token
    fwrite(&block->token, sizeof(uint8_t), 1, output_file);

    // Write all sequences
    for (size_t i = 0; i < block->sequences; i++) {
        write_sequence(block->block_sequences[i], output_file);
    }

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

void block_encode(const char* block_entry, size_t block_length, LZ4Block* block, FILE* log_file, FILE* output_file) {
    size_t input_index = 0;
    uint8_t* input = (uint8_t*)block_entry;
    LZ4Sequence current_sequence = {0};
    uint16_t literal_counter = 0;

    while (input_index < block_length) {
        uint8_t match_distance = 0;
        uint8_t match_length = find_longest_match(input, input_index, &match_distance);

        if (match_length < MIN_MATCH_LENGTH) {
            // Handle literals
            if (literal_counter == 0) {
                current_sequence.literals = &input[input_index];
            }
            input_index++;
            literal_counter++;
        } else {
            // Add a sequence with a match
            current_sequence.match_offset = match_distance;
            current_sequence.literal_length = literal_counter;
            current_sequence.match_length = match_length;

            uint8_t token_literal_length = (literal_counter >= 15) ? 15 : literal_counter;
            uint8_t token_match_length = (match_length >= 15) ? 15 : match_length - MIN_MATCH_LENGTH;
            current_sequence.token = (token_literal_length << 4) | token_match_length;

            // Add sequence to the block
            add_sequence_to_block(current_sequence, block);

            // Reset literal counter
            literal_counter = 0;
            input_index += match_length;
        }
    }

    // Handle remaining literals
    if (literal_counter > 0) {
        current_sequence.match_offset = 0;
        current_sequence.literal_length = literal_counter;
        current_sequence.match_length = 0;

        uint8_t token_literal_length = (literal_counter >= 15) ? 15 : literal_counter;
        current_sequence.token = (token_literal_length << 4);

        add_sequence_to_block(current_sequence, block);
    }

    // Set block token and write output
    block->token = block->sequences;
    write_output(block, output_file);
    print_block_details(log_file, block);
}

void free_block_sequences(LZ4Block* block) {
    if (block->block_sequences != NULL) {
        for (size_t i = 0; i < block->sequences; ++i) {
            if (block->block_sequences[i].literals != NULL) {
                free(block->block_sequences[i].literals);
                block->block_sequences[i].literals = NULL;
            }
        }
        free(block->block_sequences);
        block->block_sequences = NULL;
        block->sequences = 0;
    }
}
char** divide_input(const uint8_t* input_data, size_t input_size, size_t block_size, size_t* block_count) {
    // Calculate the number of blocks
    *block_count = (input_size + block_size - 1) / block_size;
    printf("Necessary block count: %zu\n", *block_count);

    // Allocate memory for the array of block pointers
    char** blocks = (char**)malloc(*block_count * sizeof(char*));
    if (blocks == NULL) {
        perror("Error: Unable to allocate memory for blocks");
        exit(1);
    }

    // Allocate memory for each block and copy the data
    for (size_t i = 0; i < *block_count; i++) {
        size_t current_block_size = (i == *block_count - 1) 
            ? input_size - i * block_size  // Last block may have fewer bytes
            : block_size;

        blocks[i] = (char*)malloc(current_block_size);
        if (blocks[i] == NULL) {
            perror("Error: Unable to allocate memory for a block");
            // Free all previously allocated blocks
            for (size_t j = 0; j < i; j++) {
                free(blocks[j]);
            }
            free(blocks);
            exit(1);
        }

        memcpy(blocks[i], &input_data[i * block_size], current_block_size);
    }

    return blocks;
}

void lz4_encode(LZ4Context* context, FILE* log_file, FILE* output_file) {
    size_t block_size = DEFAULT_BLOCK_LENGTH; // Default block size
    size_t block_count = 0;

    // Divide input into blocks
    char** blocks = divide_input(context->input_data, context->input_size, block_size, &block_count);

    // Log block details
    fprintf(log_file, "Encoding %zu blocks with block size %zu bytes.\n", block_count, block_size);

    // Encode each block
    for (size_t i = 0; i < block_count; i++) {
        LZ4Block currentBlock = {0}; // Initialize a new block

        // Calculate the current block's size
        size_t current_block_size = (i == block_count - 1) 
            ? context->input_size - i * block_size  // Last block may have fewer bytes
            : block_size;

        // Encode the block (use current_block_size instead of strlen)
        block_encode(blocks[i], current_block_size, &currentBlock, log_file, output_file);

        // Free block sequences to avoid memory leaks
        free_block_sequences(&currentBlock);

        // Free memory for the current block
        free(blocks[i]);
    }
    

    // Free memory for block array
    free(blocks);
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
        printf("Fatal error: no input.txt file in directory: %s", DEFAULT_INPUT_FILE);
        exit(1);
    }
}


void clear_files(){
    FILE* output_file = fopen(DEFAULT_OUTPUT_BIN_FILE, "wb");
    fclose(output_file);
    FILE* log_file_initial = fopen(DEFAULT_LOG_FILE, "w");
    fclose(log_file_initial);
}

int main() {
    ensure_directories();
    clear_files();

    LZ4Context context = {0};
    FILE* log_file = safe_open(DEFAULT_LOG_FILE, "a");
    FILE* input_file = safe_open(DEFAULT_INPUT_FILE, "r");
    FILE* output_file = safe_open(DEFAULT_OUTPUT_BIN_FILE, "ab");

    // Load input data
    fseek(input_file, 0, SEEK_END);
    long file_size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);

    context.input_data = malloc(file_size + 1);
    if (context.input_data == NULL) {
        perror("Error: Unable to allocate memory");
        fclose(input_file);
        fclose(log_file);
        return 1;
    }

    size_t bytes_read = fread(context.input_data, 1, file_size, input_file);
    fclose(input_file);

    if (bytes_read != file_size) {
        perror("Error: Failed to read input file");
        free(context.input_data);
        fclose(log_file);
        return 1;
    }

    context.input_data[file_size] = '\0';
    context.input_size = bytes_read;

    printf("File Content:\n%s\n", context.input_data);

    // Encode the data
    lz4_encode(&context, log_file, output_file);

    printf("Encoding completed. Check %s for details.\n", DEFAULT_LOG_FILE);

    // Clean up
    fclose(log_file);
    fclose(output_file);
    free(context.input_data);

    dump_to_hex_file(DEFAULT_OUTPUT_BIN_FILE, DEFAULT_OUTPUT_HEX_FILE);
    return 0;
}