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
char* state = "encoding";

typedef struct {
    uint8_t token;
    size_t byte_size;
    uint8_t* literals;
    size_t literal_length;
    uint16_t match_offset;
    size_t match_length;
} LZ4Sequence;

typedef struct {
    uint8_t token;
    size_t byte_size;
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

#pragma region encoding
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
    fprintf(log_file, "\nByte size: %u\n", sequence->byte_size);
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
    fprintf(log_file, "Block encoded:\n");
    
    // Correct format specifier for byte_size
    fprintf(log_file, "byte size: %zu\n", block->byte_size);

    // Print binary representation of the token
    print_binary_to_file(log_file, block->token);
    fprintf(log_file, "\n");

    for (size_t i = 0; i < block->sequences; i++) {
        print_sequence_details(log_file, &block->block_sequences[i]);
    }
    fprintf(log_file, "\n");
}


void print_frame_details(FILE* log_file, LZ4Frame* frame) {
    fprintf(log_file, "Compressed Frame: token %d\n", frame->blocks);
    print_binary_to_file(log_file, frame->blocks);
    fprintf(log_file, "\n");
    for (size_t i = 0; i < frame->blocks; i++) {
        print_block_details(log_file, &frame->frame_blocks[i]);
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


void write_sequence(LZ4Sequence sequence, FILE* file) {
    fwrite(&sequence.token, sizeof(uint8_t), 1, file);
    fwrite(&sequence.byte_size, sizeof(uint16_t), 1, file);
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
        }
}
}

void write_block(LZ4Block* block, FILE* output_file) {

    // Write the block token
    fwrite(&block->token, sizeof(uint8_t), 1, output_file);
    fwrite(&block->byte_size, sizeof(uint16_t), 1, output_file);

    // Write all sequences
    for (size_t i = 0; i < block->sequences; i++) {
        write_sequence(block->block_sequences[i], output_file);
    }

}

void write_output(LZ4Frame* frame, FILE* output_file) {
    fwrite(&frame->blocks, sizeof(uint8_t), 1, output_file);
    for (size_t i = 0; i < frame->blocks; i++) {
        write_block(&frame->frame_blocks[i], output_file);
        // Free block sequences immediately after writing
        free_block_sequences(&frame->frame_blocks[i]);

    }

    // Free the frame's block array
    free(frame->frame_blocks);
    frame->frame_blocks = NULL;
    frame->blocks = 0;
}

void add_sequence_to_block(LZ4Sequence seq, LZ4Block* block) {
    if(block->sequences == 0){
        block->block_sequences = malloc(sizeof(LZ4Sequence));
    } else {
        block->block_sequences = realloc(block->block_sequences, sizeof(LZ4Sequence)*(block->sequences+1));
    }
    block->block_sequences[block->sequences] = seq;
    block->sequences += 1;
    block->byte_size+= seq.byte_size;
}


void add_block_to_frame(LZ4Frame* frame, LZ4Block block) {
    // Debugging: Print the current number of blocks before reallocating
    printf("Current number of blocks: %zu\n", frame->blocks);

    // If it's the first block, initialize the frame's block array
    if (frame->blocks == 0) {
        frame->frame_blocks = malloc(sizeof(LZ4Block));  // Allocate memory for the first block
        if (frame->frame_blocks == NULL) {
            perror("Failed to allocate memory for frame_blocks");
            exit(EXIT_FAILURE);  // Exit if memory allocation fails
        }
    } else {
        // Prevent reallocation if blocks exceed a certain limit to avoid overflow
        if (frame->blocks > SIZE_MAX / sizeof(LZ4Block)) {
            fprintf(stderr, "Size overflow: Too many blocks!\n");
            exit(EXIT_FAILURE);
        }

        // Debugging: Print the reallocation size
        printf("Reallocating memory for frame_blocks: %zu blocks\n", frame->blocks + 1);

        LZ4Block* temp = realloc(frame->frame_blocks, sizeof(LZ4Block) * (frame->blocks + 1));
        if (temp == NULL) {
            // If realloc fails, free the original memory and exit
            perror("Failed to reallocate memory for frame_blocks");
            free(frame->frame_blocks);
            exit(EXIT_FAILURE);
        }
        frame->frame_blocks = temp;  // Update the frame_blocks pointer with the reallocated memory
    }

    // Add the block to the frame
    frame->frame_blocks[frame->blocks] = block;

    // Increment the block count
    frame->blocks++;

    // Debugging: Print the new number of blocks
    printf("New number of blocks: %zu\n", frame->blocks);
}



void block_encode(const char* block_entry, size_t block_length, LZ4Block* block, FILE* log_file, FILE* output_file, LZ4Frame* frame) {
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
            current_sequence.byte_size = sizeof(uint8_t)*(literal_counter+5);

            if (current_sequence.literal_length >= 15) {
                uint8_t remaining = current_sequence.literal_length - 15;
                while (remaining >= 255) {
                    current_sequence.byte_size++;
                    remaining -= 255;
                }
                current_sequence.byte_size++;
            }

            uint8_t adjusted_match_length = current_sequence.match_length - 4;

        if (adjusted_match_length >= 15) {
            uint8_t remaining = adjusted_match_length - 15;
            while (remaining >= 255) {
                current_sequence.byte_size++;
                remaining -= 255;
            }
            current_sequence.byte_size++;
        }

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

         current_sequence.byte_size = sizeof(uint8_t)*(literal_counter+5);

            if (current_sequence.literal_length >= 15) {
                uint8_t remaining = current_sequence.literal_length - 15;
                while (remaining >= 255) {
                    current_sequence.byte_size++;
                    remaining -= 255;
                }
                current_sequence.byte_size++;
            }


        add_sequence_to_block(current_sequence, block);
    }

    // Set block token and write output
    block->token = block->sequences;
    block->byte_size+=3;
    add_block_to_frame(frame, *block);  // Add block to the frame

}



char** divide_input(const uint8_t* input_data, size_t input_size, size_t block_size, size_t* block_count) {
    // Calculate the number of blocks
    *block_count = (input_size + block_size - 1) / block_size;

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

void lz4_encode(LZ4Context* context, FILE* log_file, FILE* output_file, LZ4Frame* frame) {
    size_t block_size = DEFAULT_BLOCK_LENGTH;  // Default block size
    size_t block_count = 0;

    // Divide input into blocks
    char** blocks = divide_input(context->input_data, context->input_size, block_size, &block_count);


    // Encode each block
    for (size_t i = 0; i < block_count; i++) {
        LZ4Block currentBlock = {0};  // Initialize a new block

        // Calculate the current block's size
        size_t current_block_size = (i == block_count - 1)
            ? context->input_size - i * block_size  // Last block may have fewer bytes
            : block_size;

        // Encode the block
        block_encode(blocks[i], current_block_size, &currentBlock, log_file, output_file, frame);
    }

    // After encoding, check frame.blocks to ensure the block count was updated correctly
    print_frame_details(log_file, frame);
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

void free_frame(LZ4Frame* frame) {
    if (frame == NULL) return;  // Ensure that the frame pointer is not null

    // Free each block's sequences if they exist
    for (size_t i = 0; i < frame->blocks; ++i) {
        free_block_sequences(&frame->frame_blocks[i]);
    }

    // Free the block array itself
    free(frame->frame_blocks);
    frame->frame_blocks = NULL;  // Avoid dangling pointer

    // Reset the block count
    frame->blocks = 0;
}

#pragma endregion encoding

// void remove_bytes(char** input, size_t* file_size, size_t num_bytes) {
//     if (num_bytes > *file_size) {
//         fprintf(stderr, "Error: Attempting to remove more bytes than the file size.\n");
//         return;
//     }

//     // Shift the memory
//     memmove(*input, *input + num_bytes, *file_size - num_bytes);

//     // Update the size
//     *file_size -= num_bytes;

//     // Optionally, shrink memory (if needed, using realloc)
//     *input = realloc(*input, *file_size);
//     if (!*input) {
//         perror("Error reallocating memory");
//         exit(EXIT_FAILURE);
//     }
// }

void sequence_decode(char* input_data, LZ4Sequence* seq) {
    printf("Input data: ");
    for (int i = 0; i < seq->byte_size; i++) {
        printf("%02X ", (unsigned char)input_data[i]);
    }
    printf("\n");

    // Extract token and lengths
    seq->token = input_data[0];
    size_t pointer = 3; 
    printf("Token (Hex): %02X\n", seq->token);  // Print token as hexadecimal
    seq->literal_length = (seq->token & 0xF0) >> 4;  // Upper nibble (literal length)
    seq->match_length = seq->token & 0x0F;
    if(seq->literal_length>=15){
        while(input_data[pointer]==255){
            seq->literal_length+=15;
            pointer++;
        }
        seq->literal_length+= input_data[pointer];
    }

    printf("Match length: %d\n", seq->match_length);
    printf("Literal length: %d\n", seq->literal_length);

    // Now extract literals based on the literal length
     // pointer starts right after the token (byte 0)
    char literals[seq->literal_length];  // Array to store the literals

    // Extract literals
    for (size_t i = 0; i < seq->literal_length; i++) {
        literals[i] = input_data[pointer + i];
    }
    seq->literals = literals;

    // Print literals for verification
    printf("Literals: ");
    for (size_t i = 0; i < seq->literal_length; i++) {
        printf("%02X ", (unsigned char)seq->literals[i]);
    }
    printf("\n");

    // Calculate match offset based on pointer position after literals
    pointer += seq->literal_length;
    
    seq->match_offset = input_data[pointer] + input_data[pointer+1];
    pointer+=3;

    if(seq->match_length>=15){
        while(input_data[pointer]==255){
            seq->match_length+=15;
            pointer++;
        }
        seq->match_length+= input_data[pointer];
        printf("pointer:%d\n", pointer);
    }
    seq->match_length+=4;
}


void block_decode(char* input_data, LZ4Block* block) {
    printf("Input data: ");
    for (int i = 0; i < block->byte_size; i++) {
        printf("%02X ", (unsigned char)input_data[i]);
    }
    printf("\n");

    block->token = input_data[0];  // Assuming token is a single byte
    size_t pointer = 0;  // Starting from the second byte

    for (size_t i = 0; i < block->token; i++) {
        LZ4Sequence seq;

        seq.byte_size = input_data[pointer+4] + input_data[pointer+5];
        printf("sequence byte size: %d\n",seq.byte_size);
        

        // Dynamically allocate memory for seq_data
        char *seq_data = (char *)malloc(seq.byte_size * sizeof(char));
        if (seq_data == NULL) {
            perror("Failed to allocate memory for seq_data");
            return;
        }

        // Copy the substring from input to seq_data
        memcpy(seq_data, &input_data[pointer+3], seq.byte_size);
        sequence_decode(seq_data, &seq);
        add_sequence_to_block(seq, block);

        // Adjust the pointer
        pointer += seq.byte_size;
        //block->block_sequences[i] = seq;
        // Free allocated memory for seq_data
        free(seq_data);
    }
}

void LZ4_decode(char* input_bin_file, char* log) {
    // Open the binary file for reading
    FILE* input_file = fopen(input_bin_file, "rb");
    FILE* log_file = fopen(log, "a");
    if (!input_file || !log_file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Determine the file size
    fseek(input_file, 0, SEEK_END);
    size_t file_size = ftell(input_file);
    if (file_size < 0) {
        perror("Error getting file size");
        fclose(input_file);
        fclose(log_file);
        exit(EXIT_FAILURE);
    }
    printf("File size: %zu bytes\n", file_size); // Debugging

    fseek(input_file, 0, SEEK_SET);

    // Allocate memory to store the file content
    char* input = malloc(file_size);
    if (!input) {
        perror("Error allocating memory");
        fclose(input_file);
        fclose(log_file);
        exit(EXIT_FAILURE);
    }

    // Read the file content into the buffer
    size_t bytes_read = fread(input, 1, file_size, input_file);
    if (bytes_read != file_size) {
        fprintf(stderr, "Error: Expected %zu bytes, but only read %zu bytes.\n", file_size, bytes_read);
        free(input);
        fclose(input_file);
        fclose(log_file);
        exit(EXIT_FAILURE);
    }

    // Log and print the content in hexadecimal
    printf("File content in hexadecimal:\n");
    for (size_t i = 0; i < bytes_read; i++) {
        fprintf(log_file, "%02X ", (unsigned char)input[i]);
        printf("%02X ", (unsigned char)input[i]);
    }
    printf("\n");

    // Start processing the LZ4 frame

    LZ4Frame frame_decode = {0}; 
    size_t block_count = input[0];  // Assuming the first byte is the block count
    size_t pointer = 1;  // Skip the frame token byte
    frame_decode.frame_blocks = NULL;

    frame_decode.blocks = 0;
    for (size_t i = 0; i < block_count; i++) {
        LZ4Block block = {0}; // Initialize block before use
        block.byte_size = input[pointer + 1] + input[pointer + 2]; // Calculate the block size (example logic)
        printf("Block size: %zu\n", block.byte_size);

        // Dynamically allocate memory for block_data
        char *block_data = (char *)malloc(block.byte_size);
        if (block_data == NULL) {
            perror("Failed to allocate memory for block_data");
            free(input);
            fclose(input_file);
            fclose(log_file);
            exit(EXIT_FAILURE);
        }

        // Copy the substring from input to block_data
        memcpy(block_data, &input[pointer], block.byte_size);

        // Decode the block into sequences
        block_decode(block_data, &block);
            // If it's the first block, initialize the frame's block array
printf("Before adding block, frame->blocks = %zu\n", frame_decode.blocks);
add_block_to_frame(&frame_decode, block);

printf("After adding block, frame->blocks = %zu\n", frame_decode.blocks);

    // Cleanup
    

    
}
print_frame_details(log_file, &frame_decode);
    free(input);
free_frame(&frame_decode);
}


int main() {//TODO: move to lz4encode

//compression part
#pragma region encode

    ensure_directories();
    clear_files();

    LZ4Context context = {0};
    LZ4Frame frame;
    frame.blocks=0;
    frame.frame_blocks = NULL;
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

lz4_encode(&context, log_file, output_file, &frame);

    write_output(&frame, output_file);

    printf("Encoding completed. Check %s for details.\n", DEFAULT_LOG_FILE);

    // Clean up
    fclose(log_file);
    fclose(output_file);
    free(context.input_data);
    free_frame(&frame);
    dump_to_hex_file(DEFAULT_OUTPUT_BIN_FILE, DEFAULT_OUTPUT_HEX_FILE);

#pragma endregion encode
    
//decompression part
state = "decoding";

LZ4_decode(DEFAULT_OUTPUT_BIN_FILE, DEFAULT_LOG_FILE);
    return 0;
}