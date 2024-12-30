/*  TODO:
        check for redundant code and encapsulate better
        separate the code into more part --> later modules
        comment the code
        write better logs and log file (readable comparison)
        parallelism
        handle line breaks in file
        check uint16_t
*/

#pragma region init

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <time.h>

#define MAX_MATCH_LENGTH  1024 //arbitrary values
#define MIN_MATCH_LENGTH  4
#define WINDOW_SIZE       65535 // limited by the offset measuring 2 bytes
#define DEFAULT_BLOCK_LENGTH 300 //arbitrary value, must be lower than the length of the input
#define DEFAULT_LOG_FILE "../../../Output-Input/log/encoding_log.txt"
#define DEFAULT_COMPRESSED_FILE "../../../Output-Input/out/compressed.bin"
#define DEFAULT_UNCOMPRESSED_FILE "../../../Output-Input/out/uncompressed.txt"
#define DEFAULT_INPUT_FILE "../../../Output-Input/input/input.txt"
#define DEFAULT_OUTPUT_HEX_FILE "../../../Output-Input/out/compressed.txt"

typedef struct {
    uint8_t token;
    size_t byte_size;
    uint8_t* literals;
    size_t literals_count; 
    uint16_t match_offset;
    size_t match_length;
} LZ4Sequence;

typedef struct {
    uint8_t token;
    size_t byte_size;
    size_t sequences_count;
    LZ4Sequence* sequences;
} LZ4Block;

typedef struct {
    size_t blocks;
    LZ4Block* frame_blocks;
} LZ4Frame;

typedef struct {
    uint8_t* input_data;
    size_t input_size;
} LZ4Context;

#pragma endregion init

#pragma region utils

//helper function to print a binary octet to a given file
void print_binary_to_file(FILE* file, uint8_t num) {
    for (int i = 7; i >= 0; i--) {
        fprintf(file, "%d", (num >> i) & 1);
    }
}

// void print_timestamp() {
//     time_t rawtime;
//     struct tm *timeinfo;
//     char buffer[80];

//     // Get current time
//     time(&rawtime);
    
//     // Convert time to local time format
//     timeinfo = localtime(&rawtime);
    
//     // Format the time into a human-readable string (YYYY-MM-DD HH:MM:SS)
//     strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", timeinfo);
    
//     // Print the timestamp
//     printf("Timestamp: %s\n", buffer);
// }

void dump_to_hex_file(const char *input_filename, const char *output_filename) {
    FILE *input_file = fopen(input_filename, "rb");

    if (input_file == NULL) {
        perror("Error opening input file");

        return;
    }

    FILE *output_file = fopen(output_filename, "w");

    if (output_file == NULL) {
        perror("Error opening output file");

        fclose(input_file);

        return;
    }

    uint8_t buffer; //placeholder for the current binary byte


    while (fread(&buffer, 1, 1, input_file) == 1) {
        fprintf(output_file, "%02X ", buffer);
    }

    fclose(input_file);

    fclose(output_file);
}

FILE* safe_open(char* file_name, char* mode) {
    FILE* file = fopen(file_name, mode);

    if (file == NULL) {
        perror("Error: Unable to open file");

        exit(1);
    }
    return file;
}

//divides a file into computable blocks for LZ4, and returns useful information concerning the blocks
char** divide_input(const uint8_t* input_data, size_t input_size, size_t block_size, size_t* block_count) {
    *block_count = (input_size + block_size - 1) / block_size;

    char** blocks = (char**)malloc(*block_count * sizeof(char*));

    if (blocks == NULL) {
        perror("Error: Unable to allocate memory for blocks");

        exit(1);
    }

    printf("Input size: %zu, Block size: %zu, Block count: %zu\n", input_size, block_size, *block_count);

    for (size_t i = 0; i < *block_count; i++) {
        size_t current_block_size = (i == *block_count - 1) ? input_size - i * block_size : block_size;

        if (current_block_size == 0 || current_block_size > input_size) { // handles edge cases
            fprintf(stderr, "Error: Invalid block size at block %zu. Current block size: %zu\n", i, current_block_size);

            for (size_t j = 0; j < i; j++) {
                free(blocks[j]);
            }

            free(blocks);

            exit(1);
        }

        blocks[i] = (char*)malloc(current_block_size);

        if (blocks[i] == NULL) {
            perror("Error: Unable to allocate memory for a block");
            for (size_t j = 0; j < i; j++) {
                free(blocks[j]);
            }

            free(blocks);

            exit(1);
        }

        memcpy(blocks[i], &input_data[i * block_size], current_block_size); //copies the corresponding portion of the input into the block data

        printf("Block %zu allocated with size %zu\n", i, current_block_size);
    }

    return blocks;
}


void ensure_directories() {
    if (_mkdir("../../../Output-Input") == 0) {
        perror("Warning: File inexistant, new file instantiated: ");
    }

    if (_mkdir("../../../Output-Input/bin") == 0) {
        perror("Warning: File inexistant, new file instantiated: ");
    }

    if (_mkdir("../../../Output-Input/log") == 0) {
        perror("Warning: File inexistant, new file instantiated: ");
    }

    if (_mkdir("../../../Output-Input/input") == 0) { // an input file is necessary, so fatal error if inexistant
        printf("Fatal error: no input.txt file in directory: %s", DEFAULT_INPUT_FILE);

        exit(1);
    }
}

void clear_files(){
    FILE* output_file = fopen(DEFAULT_COMPRESSED_FILE, "wb");

    fclose(output_file);

    FILE* log_file_initial = fopen(DEFAULT_LOG_FILE, "w");

    fclose(log_file_initial);
}

#pragma endregion utils

#pragma region LZ4_API

//logs all the metadata of a LZ4 sequence into the output
void print_sequence_details(FILE* log_file, LZ4Sequence* sequence) {
    fprintf(log_file, "\n");

    print_binary_to_file(log_file, sequence->token);

    fprintf(log_file, "\nByte size: %u\n", sequence->byte_size);

    fprintf(log_file, "\nMatch Offset: %u\n", sequence->match_offset);

    fprintf(log_file, "Match Length: %zu\n", sequence->match_length);

    fprintf(log_file, "Literal Length: %zu\n", sequence->literals_count);
    
    fprintf(log_file, "Literals: ");

    for (size_t i = 0; i < sequence->literals_count; i++) {
        if (sequence->literals[i] >= 32 && sequence->literals[i] <= 126) { //printable characters
            fprintf(log_file, "%c", sequence->literals[i]);
        } else { //non printable characters
            fprintf(log_file, "0x%02X", sequence->literals[i]);
        }
    }

    fprintf(log_file, "\n");
}

//prints all the metadata concerning a LZ4 block
void print_block_details(FILE* log_file, LZ4Block* block) {
    fprintf(log_file, "Block encoded:\n");
    
    printf("block->byte_size before print_block_details: %zu\n", block->byte_size);

    fprintf(log_file, "encoded %zu bytes\n", block->byte_size);
    
    print_binary_to_file(log_file, block->token);

    fprintf(log_file, "\n");

    for (size_t i = 0; i < block->sequences_count; i++) {
        print_sequence_details(log_file, &block->sequences[i]);
    }

    fprintf(log_file, "\n");
}

//prints all the metadata concerning a LZ4 frame
void print_frame_details(FILE* log_file, LZ4Frame* frame) {
    fprintf(log_file, "Compressed Frame: token %d\n", frame->blocks);

    print_binary_to_file(log_file, frame->blocks);

    fprintf(log_file, "\n");

    for (size_t i = 0; i < frame->blocks; i++) {
        print_block_details(log_file, &frame->frame_blocks[i]);
    }

    fprintf(log_file, "\n");
}

//scans the search buffer to find a corresponding sequence of literals, returns the offset and the length
uint8_t find_longest_match(uint8_t* input, size_t current_index, uint8_t* match_distance) {
    size_t longest_match_length = 0;

    size_t longest_match_distance = 0;

    size_t window_start = (current_index >= WINDOW_SIZE) ? current_index - WINDOW_SIZE : 0; // handles the case were the search buffer is shorter that its expected length

    for (size_t i = window_start; i < current_index; ++i) {
        size_t current_match_length = 0;

        while (current_match_length < MAX_MATCH_LENGTH && (input[i + current_match_length] == input[current_index + current_match_length])) {
            current_match_length++;
        }

        if (current_match_length > longest_match_length) {
            longest_match_length = current_match_length;

            longest_match_distance = current_index - i;
        }
    }

    if (longest_match_length >= MIN_MATCH_LENGTH) { //ensure a lower bound to ensure compressio and not extension
        *match_distance = (uint8_t)longest_match_distance;

        return (uint8_t)longest_match_length;
    } else {
        return 0;
    }
}

void free_sequences(LZ4Block* block) {
    if (block->sequences != NULL) {
        for (size_t i = 0; i < block->sequences_count; ++i) {
            if (block->sequences[i].literals != NULL) {
                free(block->sequences[i].literals);

                block->sequences[i].literals = NULL;
            }
        }

        free(block->sequences);

        block->sequences = NULL; //dereferences the pointer

        block->sequences_count = 0;
    }
}

void free_frame(LZ4Frame* frame) {
    if (frame == NULL) return; 
    printf("frameblocks: %zu\n", frame->blocks);
    for (size_t i = 0; i < frame->blocks; ++i) {
        printf("freeing block %d\n", i);
        free_sequences(&frame->frame_blocks[i]);
    }

    free(frame->frame_blocks);

    frame->frame_blocks = NULL; //dereferences the pointer

    frame->blocks = 0;
}

void write_sequence(LZ4Sequence sequence, FILE* file) {
    fwrite(&sequence.token, sizeof(uint8_t), 1, file);

    fwrite(&sequence.byte_size, sizeof(uint16_t), 1, file);
    
    if (sequence.literals_count >= 15) {
        uint8_t remaining = sequence.literals_count - 15;

        while (remaining >= 255) {
            uint8_t to_write = 255;

            fwrite(&to_write, sizeof(uint8_t), 1, file);

            remaining -= 255;
        }

        fwrite(&remaining, sizeof(uint8_t), 1, file);
    }

    fwrite(sequence.literals, sizeof(uint8_t), sequence.literals_count, file);

    fwrite(&sequence.match_offset, sizeof(uint16_t), 1, file);

    if (sequence.match_length >= 4) {
        uint8_t adjusted_match_length = sequence.match_length - 4;

        if (adjusted_match_length >= 15) { //handles the case where the match length excedes what can be written in the token, if match_length = 15, the 0 in next byte needs to be written anyway
            uint8_t remaining = adjusted_match_length - 15;

            while (remaining >= 255) {
                uint8_t to_write = 255;

                fwrite(&to_write, sizeof(uint8_t), 1, file);

                remaining -= 255;
            }

            fwrite(&remaining, sizeof(uint8_t), 1, file); // writes the last incomplete byte or the only incomplete byte
        }
    }
}

void write_block(LZ4Block* block, FILE* output_file) {
    fwrite(&block->token, sizeof(uint8_t), 1, output_file);

    fwrite(&block->byte_size, sizeof(uint16_t), 1, output_file);

    for (size_t i = 0; i < block->sequences_count; i++) {
        write_sequence(block->sequences[i], output_file);
    }
}

void write_output(LZ4Frame* frame, FILE* output_file) {
    fwrite(&frame->blocks, sizeof(uint8_t), 1, output_file);

    for (size_t i = 0; i < frame->blocks; i++) {
        write_block(&frame->frame_blocks[i], output_file);
    }
    
    free(frame->frame_blocks);

    frame->frame_blocks = NULL;

    frame->blocks = 0;
}

void add_sequence_to_block(LZ4Sequence seq, LZ4Block* block) {
    if(block->sequences_count == 0){
        block->sequences = malloc(sizeof(LZ4Sequence));
    } else {
        block->sequences = realloc(block->sequences, sizeof(LZ4Sequence)*(block->sequences_count+1));
    }

    block->sequences[block->sequences_count] = seq;

    block->sequences_count += 1;

    block->byte_size+= seq.byte_size;
}

void add_block_to_frame(LZ4Frame* frame, LZ4Block block) {
    printf("Current number of blocks: %zu\n", frame->blocks);

    if (frame->blocks == 0) {
        frame->frame_blocks = malloc(sizeof(LZ4Block));  

        if (frame->frame_blocks == NULL) {
            perror("Failed to allocate memory for frame_blocks");

            exit(EXIT_FAILURE);  
        }
    } else {
        if (frame->blocks > SIZE_MAX / sizeof(LZ4Block)) {
            fprintf(stderr, "Size overflow: Too many blocks!\n");

            exit(EXIT_FAILURE);
        }

        printf("Reallocating memory for frame_blocks: %zu blocks\n", frame->blocks + 1);

        LZ4Block* temp = realloc(frame->frame_blocks, sizeof(LZ4Block) * (frame->blocks + 1));

        if (temp == NULL) {
            perror("Failed to reallocate memory for frame_blocks");

            free(frame->frame_blocks);

            exit(EXIT_FAILURE);
        }

        frame->frame_blocks = temp;  
    }

    frame->frame_blocks[frame->blocks] = block;

    frame->blocks++;
}

void block_encode(const char* block_entry, size_t block_length, LZ4Block* block, FILE* log_file, FILE* output_file, LZ4Frame* frame) {
    size_t input_index = 0;

    uint8_t* input = (uint8_t*)block_entry;

    LZ4Sequence current_sequence = {0};

    uint16_t literal_counter = 0;

    while (input_index < block_length) {
        uint8_t match_distance = 0;

        uint8_t match_length = find_longest_match(input, input_index, &match_distance);

        if (match_length == 0) { // writes the literal value
            if (literal_counter == 0) {
                current_sequence.literals = &input[input_index];
            }

            input_index++;

            literal_counter++;
        } else { //writes the match
            current_sequence.match_offset = match_distance;

            current_sequence.literals_count = literal_counter;

            current_sequence.match_length = match_length;

            uint8_t token_literals_count = (literal_counter >= 15) ? 15 : literal_counter;

            uint8_t token_match_length = (match_length >= 15) ? 15 : match_length - MIN_MATCH_LENGTH; //actual match length is max 19 in the token, gain of 4 bits because of the lower limit

            current_sequence.token = (token_literals_count << 4) | token_match_length;

            current_sequence.byte_size = sizeof(uint8_t)*(literal_counter+5);

            if (current_sequence.literals_count >= 15) { //handles the case where the literal length excedes 15 and adjust the byte_size accordingly
                uint8_t remaining = current_sequence.literals_count - 15;

                while (remaining >= 255) {
                    current_sequence.byte_size++;

                    remaining -= 255;
                }

                current_sequence.byte_size++;
            }

            uint8_t adjusted_match_length = current_sequence.match_length - 4;

        if (adjusted_match_length >= 15) { //handles the case where the match length excedes 15 and adjust the byte_size accordingly
            uint8_t remaining = adjusted_match_length - 15;

            while (remaining >= 255) {
                current_sequence.byte_size++;

                remaining -= 255;
            }
            current_sequence.byte_size++;
        }   

        add_sequence_to_block(current_sequence, block);

        literal_counter = 0;

        input_index += match_length;
        }
    }

    if (literal_counter > 0) { // handles the case where there is not match at the end of a block, and encodes all the literals with match length 0
        current_sequence.match_offset = 0;

        current_sequence.literals_count = literal_counter;

        current_sequence.match_length = 0;

        uint8_t token_literals_count = (literal_counter >= 15) ? 15 : literal_counter;

        current_sequence.token = (token_literals_count << 4);

         current_sequence.byte_size = sizeof(uint8_t)*(literal_counter+5);

            if (current_sequence.literals_count >= 15) {
                uint8_t remaining = current_sequence.literals_count - 15;

                while (remaining >= 255) {
                    current_sequence.byte_size++;

                    remaining -= 255;
                }
                current_sequence.byte_size++;
            }

        add_sequence_to_block(current_sequence, block);
    }

    block->token = block->sequences_count;

    block->byte_size+=3;

    add_block_to_frame(frame, *block); 
}

LZ4Context extract_uncompressed_file(FILE* file){
    LZ4Context context = {0};

    fseek(file, 0, SEEK_END);

    long file_size = ftell(file);

    fseek(file, 0, SEEK_SET);

    if(file_size<DEFAULT_BLOCK_LENGTH){
        printf("Error: default block length is too high, please reduce it before proceding.");

        exit(1);
    }

    context.input_data = malloc(file_size + 1);

    if (context.input_data == NULL) {
        perror("Error: Unable to allocate memory");

        fclose(file);

        exit(1);
    }

    size_t bytes_read = fread(context.input_data, 1, file_size, file);

    context.input_size = bytes_read;

    fclose(file);

    if (bytes_read != file_size) {
        perror("Error: Failed to read input file");

        free(context.input_data);

        exit(1);
    }

    context.input_data[file_size] = '\0';

    return context;
}

void lz4_encode() {
    if(DEFAULT_BLOCK_LENGTH == 500){
        printf("Error: block length cannot have the value 500");

        exit(1);
    }

    printf("Encoding started\n");

    ensure_directories();

    FILE* log_file = safe_open(DEFAULT_LOG_FILE, "a");

    FILE* input_file = safe_open(DEFAULT_INPUT_FILE, "r");

    FILE* output_file = safe_open(DEFAULT_COMPRESSED_FILE, "ab");

    LZ4Context context = extract_uncompressed_file(input_file);

    printf("File extracted. Input size: %zu bytes\n", context.input_size);

    LZ4Frame frame;

    frame.blocks = 0;

    frame.frame_blocks = NULL;

    size_t block_size = DEFAULT_BLOCK_LENGTH;

    size_t block_count = 0;

     char** blocks = divide_input(context.input_data, context.input_size, block_size, &block_count);
     
     printf("Input divided into %zu blocks\n", block_count);

    for (size_t i = 0; i < block_count; i++) {
        printf("Processing block %zu\n", i);

        LZ4Block currentBlock = {0};
        size_t current_block_size = (i == block_count - 1) ? context.input_size - i * block_size : block_size;

        if (blocks[i] == NULL) {
            fprintf(stderr, "Error: Block %zu is NULL\n", i);
            continue; // Skip this block to prevent further issues
        }

        block_encode(blocks[i], current_block_size, &currentBlock, log_file, output_file, &frame);
    }

    printf("Blocks encoded\n");

    print_frame_details(log_file, &frame);

    write_output(&frame, output_file); //write the encoded sequence inside of the output file

    printf("Output written\n");

    fclose(log_file);

    fclose(output_file);

    free(context.input_data);

    free_frame(&frame);

    dump_to_hex_file(DEFAULT_COMPRESSED_FILE, DEFAULT_OUTPUT_HEX_FILE);

    printf("Encoding completed. Check %s for details.\n", DEFAULT_LOG_FILE);
}

void sequence_decode(char* input_data, LZ4Sequence* seq) {
    printf("Input data: ");

    for (int i = 0; i < seq->byte_size; i++) {
        printf("%02X ", (unsigned char)input_data[i]);
    }

    printf("\n");

    seq->token = input_data[0];

    size_t pointer = 3; 

    printf("Token (Hex): %02X\n", seq->token); 

    seq->literals_count = (seq->token & 0xF0) >> 4;

    seq->match_length = seq->token & 0x0F;
    
    if (seq->literals_count >= 15) {
        while (input_data[pointer] == 255) {
            seq->literals_count += 255;

            pointer++;
        }
        seq->literals_count += input_data[pointer];

        pointer++;
    }

    printf("Match length: %d\n", seq->match_length);

    printf("Literal length: %d\n", seq->literals_count);

    if (seq->literals_count > 0) {
        seq->literals = (char*)malloc(seq->literals_count * sizeof(char));

        if (seq->literals == NULL) {
            perror("Failed to allocate memory for literals");

            return;
        }
    } else {
        seq->literals = NULL;  
    }

    for (size_t i = 0; i < seq->literals_count; i++) {
        seq->literals[i] = input_data[pointer + i];
    }

    pointer += seq->literals_count;

    printf("Literals: ");

    for (size_t i = 0; i < seq->literals_count; i++) {
        if (seq->literals[i] >= 32 && seq->literals[i] <= 126) {
            printf("%c", seq->literals[i]);
        } else {
            printf("0x%02X", (unsigned char)seq->literals[i]);
        }
    }

    printf("\n");


    seq->match_offset = (uint16_t)(input_data[pointer] + (input_data[pointer + 1] << 8));

    pointer += 2; 

    if (seq->match_length >= 15) {
        while (input_data[pointer] == 255) {
            seq->match_length += 255; 

            pointer++; 
        }

        seq->match_length += (size_t)(unsigned char)input_data[pointer];

        printf("Adding input_data[pointer] = %X (decimal: %zu), new match_length: %zu\n", (unsigned char)input_data[pointer], (size_t)(unsigned char)input_data[pointer], seq->match_length);
        pointer++; 
    }

seq->match_length += 4; // to find the true max length (relative to the min length)
}

void block_decode(char* input_data, LZ4Block* block) {
    printf("Input data: ");

    for (int i = 0; i < block->byte_size; i++) {
        printf("%02X ", (unsigned char)input_data[i]);
    }

    printf("\n");

    block->token = input_data[0]; 

    size_t pointer = 0; 

    for (size_t i = 0; i < block->token; i++) {
        LZ4Sequence seq;

        seq.byte_size = (uint16_t)(input_data[pointer + 4]) + ((uint16_t)(input_data[pointer + 5]) << 8);

        printf("sequence byte size: %d\n",seq.byte_size);
        
        char *seq_data = (char *)malloc(seq.byte_size * sizeof(char));

        if (seq_data == NULL) {
            perror("Failed to allocate memory for seq_data");

            return;
        }

        memcpy(seq_data, &input_data[pointer+3], seq.byte_size);

        sequence_decode(seq_data, &seq);

        add_sequence_to_block(seq, block);

        pointer += seq.byte_size;
        
        free(seq_data);
    }
}

char* extract_compressed_bin_file(FILE* file){
    fseek(file, 0, SEEK_END);

    size_t file_size = ftell(file);

    if (file_size < 0) {
        perror("Error getting file size");

        fclose(file);

        exit(EXIT_FAILURE);
    }

    printf("File size: %zu bytes\n", file_size); 

    fseek(file, 0, SEEK_SET);

    char* input = malloc(file_size);

    if (!input) {
        perror("Error allocating memory");

        fclose(file);

        exit(EXIT_FAILURE);
    }

    size_t bytes_read = fread(input, 1, file_size, file);

    if (bytes_read != file_size) {
        fprintf(stderr, "Error: Expected %zu bytes, but only read %zu bytes.\n", file_size, bytes_read);

        free(input);

        fclose(file);

        fclose(file);

        exit(EXIT_FAILURE);
    }

    return input;
}

void interpret_sequence(LZ4Sequence sequence) {
    // Ensure that sequence literals are valid and initialized
    if (sequence.literals == NULL || sequence.literals_count == 0) {
        return;
    }

    if(sequence.match_length != 0){
        printf("literals_count: %zu, match_length: %zu\n", sequence.literals_count, sequence.match_length);

        size_t decompressed_size = sequence.literals_count + sequence.match_length;

        if (sequence.literals_count > SIZE_MAX - sequence.match_length) {
            printf("Overflow detected: literals_count + match_length exceeds SIZE_MAX.\n");

            return;
        }

        uint8_t* decompressed = (uint8_t*)malloc(decompressed_size);

        if (decompressed == NULL) {
            printf("NULL");

            printf("decompressed size : %zu \n", decompressed_size);

            return;
        }

        size_t current_pos = 0;

        for (size_t i = 0; i < sequence.literals_count; i++) {
            if (current_pos < decompressed_size) {
                decompressed[current_pos++] = sequence.literals[i];
            } else {
                break;
            }
        }

        for (size_t i = 0; i < sequence.match_length; i++) {
            size_t match_pos = current_pos - sequence.match_offset;
            if (match_pos < current_pos && current_pos < decompressed_size) {
                decompressed[current_pos++] = decompressed[match_pos++];
            }
        }

        FILE* uncompressed = safe_open(DEFAULT_UNCOMPRESSED_FILE, "a");

        if (uncompressed == NULL) {
            free(decompressed);

            return;
        }

        for (size_t i = 0; i < current_pos; i++) { //write the buffer into the file
            if (decompressed[i] >= 32 && decompressed[i] <= 126) {
                fprintf(uncompressed, "%c", decompressed[i]);
            } else {
                fprintf(uncompressed, "0x%02X", decompressed[i]);
            }
        }

        fclose(uncompressed);

        free(decompressed);
        }else{ //case where there is no match a the end of the sequence
        size_t decompressed_size = sequence.literals_count;

        uint8_t* decompressed = (uint8_t*)malloc(decompressed_size);
        
        size_t current_pos = 0;

        for (size_t i = 0; i < sequence.literals_count; i++) {
            if (current_pos < decompressed_size) {
                decompressed[current_pos++] = sequence.literals[i];
            }
        }

        FILE* uncompressed = safe_open(DEFAULT_UNCOMPRESSED_FILE, "a");
        if (uncompressed == NULL) {
            free(decompressed);

            return;
        }

        for (size_t i = 0; i < current_pos; i++) {
            if (decompressed[i] >= 32 && decompressed[i] <= 126) {
                fprintf(uncompressed, "%c", decompressed[i]);
            } else {
                fprintf(uncompressed, "0x%02X", decompressed[i]);
            }
        }

        fclose(uncompressed);

        free(decompressed);
    }
}

void interpret_frame(LZ4Frame frame){
    FILE* uncompressed = safe_open(DEFAULT_UNCOMPRESSED_FILE, "w");

    fclose(uncompressed);

    for(size_t i = 0; i<frame.blocks;i++){
        for(size_t j =0; j<frame.frame_blocks[i].sequences_count;j++){
            interpret_sequence(frame.frame_blocks[i].sequences[j]);
        }
    }
}

void LZ4_decode(char* input_bin_file, char* log) {
    if (DEFAULT_BLOCK_LENGTH == 500) {
        printf("Error: block length cannot have the value 500");

        exit(1);
    }

    ensure_directories();

    FILE* input_file = safe_open(input_bin_file, "rb");

    FILE* log_file = safe_open(log, "a");

    LZ4Frame frame_decode = {0};

    char* input = extract_compressed_bin_file(input_file);

    size_t block_count = input[0]; 

    size_t pointer = 1;  

    frame_decode.frame_blocks = NULL;

    frame_decode.blocks = 0;

    for (size_t i = 0; i < block_count; i++) {
        LZ4Block block = {0}; 

        uint8_t byte1 = (uint8_t)input[pointer + 1];

        uint8_t byte2 = (uint8_t)input[pointer + 2];

        size_t byte_size = byte1 + ((size_t)byte2 << 8); 

        if (byte_size <= 0) {
            printf("Error: invalid block size at block %zu\n", i);

            exit(EXIT_FAILURE);
        }

        char* block_data = (char*)malloc(byte_size);

        if (block_data == NULL) {
            perror("Failed to allocate memory for block_data");

            free(input);

            fclose(input_file);

            fclose(log_file);

            exit(EXIT_FAILURE);
        }

        memcpy(block_data, &input[pointer], byte_size);

        block_decode(block_data, &block);

        block.byte_size += 3; 

        add_block_to_frame(&frame_decode, block);

        free(block_data);

        pointer += byte_size;
    }

    print_frame_details(log_file, &frame_decode);

    free(input);

    interpret_frame(frame_decode);

    free_frame(&frame_decode);

    fclose(input_file);
    
    fclose(log_file);
}

#pragma endregion LZ4_API

int main() {
    ensure_directories();

    clear_files();

    lz4_encode();

    LZ4_decode(DEFAULT_COMPRESSED_FILE, DEFAULT_LOG_FILE);

    return 0;
}