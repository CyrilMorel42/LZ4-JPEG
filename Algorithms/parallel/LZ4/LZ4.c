/*  TODO:
        separate the code into more part --> later modules
        write better logs and log file (readable comparison) and remove excess code
        parallelism for output write
        handle line breaks in file
        différencier les zones critiques
*/

#pragma region init

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <windows.h>
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
CRITICAL_SECTION block_plus;
CRITICAL_SECTION add_seq;
CRITICAL_SECTION seq_decode;
CRITICAL_SECTION block_cs;




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

typedef struct {
    const char* block_entry;
    size_t block_length;
    LZ4Block* block;
    FILE* log_file;
    FILE* output_file;
    LZ4Frame* frame;
    size_t index;
} BlockEncodeArgs;

typedef struct {
    LZ4Block* block;
    char* blockData;
    size_t block_size;
    LZ4Frame* frame;
    size_t index;
} BlockDecodeArgs;



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

   // printf("Input size: %zu, Block size: %zu, Block count: %zu\n", input_size, block_size, *block_count);

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

       // printf("Block %zu allocated with size %zu\n", i, current_block_size);
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

//scans the search buffer to find a corresponding sequence of literals, returns the offset and the length
uint8_t find_longest_match(uint8_t* input, size_t current_index, uint16_t* match_distance) {
    size_t longest_match_length = 0;
    size_t longest_match_distance = 0;

    size_t window_start = (current_index >= WINDOW_SIZE) ? current_index - WINDOW_SIZE : 0;

    for (size_t i = window_start; i < current_index; ++i) {
        size_t current_match_length = 0;

        while (current_match_length < MAX_MATCH_LENGTH &&
               (input[i + current_match_length] == input[current_index + current_match_length])) {
            current_match_length++;
        }

        if (current_match_length > longest_match_length) {
            longest_match_length = current_match_length;
            longest_match_distance = current_index - i;
        }
    }

    if (longest_match_length >= MIN_MATCH_LENGTH) {
        *match_distance = (uint16_t)longest_match_distance;  
        return (uint8_t)longest_match_length;
    } else {
        return 0;
    }
}

//prints all the metadata concerning a LZ4 block
void print_block_details(FILE* log_file, LZ4Block* block) {
    fprintf(log_file, "Block encoded:\n");
    
    printf("block->byte_size before print_block_details: %zu\n", block->byte_size);

    fprintf(log_file, "encoded %zu bytes\n", block->byte_size);
    
    print_binary_to_file(log_file, block->token);

    fprintf(log_file, "\n");

    // for (size_t i = 0; i < block->sequences_count; i++) {
    //     print_sequence_details(log_file, &block->sequences[i]);
    // }

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
    //printf("frameblocks: %zu\n", frame->blocks);
    for (size_t i = 0; i < frame->blocks; ++i) {
       // printf("freeing block %d\n", i);
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
    // Log the start of the function call
    // printf("add_sequence_to_block called.\n");
    // printf("Current sequences count: %zu\n", block->sequences_count);

    // Log the sequence to be added
    //printf("Adding sequence with byte_size: %zu\n", seq.byte_size);

    // Allocate or reallocate memory for the sequences array
    LZ4Sequence* temp_sequences;
    if (block->sequences_count == 0) {
        // First allocation: allocate space for one sequence
        //printf("Allocating memory for the first sequence.\n");
        temp_sequences = malloc(sizeof(LZ4Sequence));
        if (temp_sequences == NULL) {
            fprintf(stderr, "Memory allocation failed for sequences array.\n");
            exit(1);
        }
    } else {
        // Reallocate: increase size by 1 each time (consider allocating more for efficiency)
        size_t new_size = block->sequences_count + 1;
        //printf("Reallocating memory. New size: %zu\n", new_size);
        temp_sequences = realloc(block->sequences, sizeof(LZ4Sequence) * new_size);

        // Handle realloc failure
        if (temp_sequences == NULL) {
            // Print an error message and exit if realloc fails
            fprintf(stderr, "Memory reallocation failed for sequences array.\n");
            // Free the previously allocated memory and exit
            free(block->sequences);
            exit(1);
        }
    }

    // Assign the resized memory back to block->sequences
    block->sequences = temp_sequences;

    // Log the successful allocation/reallocation
    //printf("Memory allocation/reallocation successful. Sequences count: %zu\n", block->sequences_count + 1);

    // Add the new sequence to the block
    block->sequences[block->sequences_count] = seq;
    //printf("Sequence added to block. Sequence index: %zu\n", block->sequences_count);

    // Update the sequence count and byte size
    block->sequences_count += 1;
    block->byte_size += seq.byte_size;

    // Log the updated values
    //printf("Updated sequences_count: %zu\n", block->sequences_count);
    //printf("Updated byte_size: %zu\n", block->byte_size);
}



void add_block_to_frame(LZ4Frame* frame, LZ4Block block) {
    //printf("Current number of blocks: %zu\n", frame->blocks);

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

        //printf("Reallocating memory for frame_blocks: %zu blocks\n", frame->blocks + 1);

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

void parallel_add_block_to_frame(LZ4Frame* frame, LZ4Block block, size_t index) {
    if (frame == NULL) {
        fprintf(stderr, "Error: frame pointer is NULL\n");
        exit(1);
    }


    //printf("index: %zu\n", index);

    frame->frame_blocks[index] = block;

    //printf("frame->blocks before increment: %zu\n", frame->blocks);
    
    // Add the block count increment here inside the critical section
    EnterCriticalSection(&block_plus);
    frame->blocks++;
    LeaveCriticalSection(&block_plus);
    
    //printf("frame->blocks after increment: %zu\n", frame->blocks);
}



DWORD WINAPI parallel_block_encode(LPVOID lpParam) {
    BlockEncodeArgs* args = (BlockEncodeArgs*)lpParam;  // Get the thread arguments

    // Check if the arguments are valid
    if (args == NULL) {
        fprintf(stderr, "Invalid thread arguments passed.\n");
        return 1;
    }

    size_t input_index = 0;
    uint8_t* input = (uint8_t*)args->block_entry;
    LZ4Sequence current_sequence = {0};
    uint16_t literal_counter = 0;
    size_t block_length = args->block_length;
    LZ4Block* block = args->block;
    FILE* log_file = args->log_file;
    FILE* output_file = args->output_file;
    LZ4Frame* frame = args->frame;
    block->sequences_count=0;
    block->byte_size=0;


    // Use Critical Section to protect access to the shared block
    

    // Encoding logic 
    while (input_index < block_length) {
        uint16_t match_distance = 0;  
        uint8_t match_length = find_longest_match(input, input_index, &match_distance);

        if (match_length == 0) { // write the literal value
            if (literal_counter == 0) {
                current_sequence.literals = &input[input_index];
            }

            input_index++;
            literal_counter++;
        } else { // write the match
            current_sequence.match_offset = match_distance;
            current_sequence.literals_count = literal_counter;
            current_sequence.match_length = match_length;

            uint8_t token_literals_count = (literal_counter >= 15) ? 15 : literal_counter;
            uint8_t token_match_length = (match_length >= 19) ? 15 : match_length - MIN_MATCH_LENGTH;

            current_sequence.token = (token_literals_count << 4) | token_match_length;
            current_sequence.byte_size = sizeof(uint8_t)*(literal_counter + 5);

            if (current_sequence.literals_count >= 15) {
                uint8_t remaining = current_sequence.literals_count - 15;
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
            EnterCriticalSection(&add_seq);  // Lock the critical section
            // Add sequence to block (protected by the critical section)
            add_sequence_to_block(current_sequence, block);
            LeaveCriticalSection(&add_seq);
            literal_counter = 0;
            input_index += match_length;
        }
    }

    if (literal_counter > 0) {
        current_sequence.match_offset = 0;
        current_sequence.literals_count = literal_counter;
        current_sequence.match_length = 0;

        uint8_t token_literals_count = (literal_counter >= 15) ? 15 : literal_counter;
        current_sequence.token = (token_literals_count << 4);
        current_sequence.byte_size = sizeof(uint8_t)*(literal_counter + 5);

        if (current_sequence.literals_count >= 15) {
            uint8_t remaining = current_sequence.literals_count - 15;
            while (remaining >= 255) {
                current_sequence.byte_size++;
                remaining -= 255;
            }
            current_sequence.byte_size++;
        }

        // Add final sequence to block (protected by the critical section)
        EnterCriticalSection(&add_seq);  // Lock the critical section
            // Add sequence to block (protected by the critical section)
            
            add_sequence_to_block(current_sequence, block);
            LeaveCriticalSection(&add_seq);
    }

    block->token = block->sequences_count;
    block->byte_size += 3;

    
    parallel_add_block_to_frame(args->frame, *block, args->index);

    free(args);

    return 0;  // Return success
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
#define CHECK_ALLOCATION(ptr) if (ptr == NULL) { \
    fprintf(stderr, "Memory allocation failed at %s:%d\n", __FILE__, __LINE__); \
    exit(1); \
}

void parallel_LZ4_encode() {
    if (DEFAULT_BLOCK_LENGTH == 500) {
        printf("Error: block length cannot have the value 500");
        exit(1);
    }

    //printf("Encoding started\n");

    //ensure_directories();

    FILE* log_file = safe_open(DEFAULT_LOG_FILE, "a");
    FILE* input_file = safe_open(DEFAULT_INPUT_FILE, "r");
    FILE* output_file = safe_open(DEFAULT_COMPRESSED_FILE, "ab");

    LZ4Context context = extract_uncompressed_file(input_file);
    //printf("File extracted. Input size: %zu bytes\n", context.input_size);

    LZ4Frame frame;
    frame.blocks = 0;
    frame.frame_blocks;

    size_t block_size = DEFAULT_BLOCK_LENGTH;
    size_t block_count = 0;

    char** blocks = divide_input(context.input_data, context.input_size, block_size, &block_count);
    //printf("Input divided into %zu blocks\n", block_count);

   
    frame.frame_blocks = malloc(sizeof(LZ4Block) * block_count);
if (frame.frame_blocks == NULL) {
    fprintf(stderr, "Error: Memory allocation failed for frame_blocks\n");
    exit(1);
}

    HANDLE* threads = malloc(block_count * sizeof(HANDLE));
    CHECK_ALLOCATION(threads);

    DWORD* threads_IDs = malloc(block_count * sizeof(DWORD));
    CHECK_ALLOCATION(threads_IDs);


// Initialize the critical section before creating threads
    InitializeCriticalSection(&block_cs);

    for (size_t i = 0; i < block_count; i++) {
        //printf("Processing block %zu\n", i);

        // Prepare arguments for each thread
        BlockEncodeArgs* args = malloc(sizeof(BlockEncodeArgs));
        CHECK_ALLOCATION(args);

        args->block_entry = blocks[i];
        args->block_length = (i == block_count - 1) ? context.input_size - i * block_size : block_size;
        args->block = malloc(sizeof(LZ4Block));  // Allocate memory for block
        CHECK_ALLOCATION(args->block);

        args->log_file = log_file;
        args->output_file = output_file;
        args->frame = &frame;
        args->index = i;

        // Create a thread for processing each block
        threads[i] = CreateThread(NULL, 0, parallel_block_encode, args, 0, &threads_IDs[i]);

        if (threads[i] == NULL) {
            fprintf(stderr, "Error: Failed to create thread for block %zu\n", i);
            exit(1);
        }

    }

    // Wait for all threads to finish
    WaitForMultipleObjects(block_count, threads, TRUE, INFINITE);

    // Clean up thread resources
    for (size_t i = 0; i < block_count; i++) {
        CloseHandle(threads[i]);
    }

    free(threads);
    free(threads_IDs);

    DeleteCriticalSection(&block_cs);

    //printf("Blocks encoded\n");

    // print_frame_details(log_file, &frame);
     write_output(&frame, output_file); // Write the encoded sequence to the output file
     //printf("Output written\n");

    fclose(log_file);
    fclose(output_file);

    free(context.input_data);
    free_frame(&frame);


    dump_to_hex_file(DEFAULT_COMPRESSED_FILE, DEFAULT_OUTPUT_HEX_FILE);
    //printf("Encoding completed. Check %s for details.\n", DEFAULT_LOG_FILE);
}


void sequence_decode(char* input_data, LZ4Sequence* seq) {
    //printf("Input data: ");
    // for (int i = 0; i < seq->byte_size; i++) {
    //     printf("%02X ", (unsigned char)input_data[i]);
    // }
    // printf("\n");

    seq->token = input_data[0];
    size_t pointer = 3;  // Start after the token

    //printf("Token (Hex): %02X\n", seq->token);

    seq->literals_count = (seq->token & 0xF0) >> 4;
    seq->match_length = seq->token & 0x0F;

    // Handling literals_count when >= 15
    if (seq->literals_count >= 15) {
        while (input_data[pointer] == 255) {
            seq->literals_count += 255;
            pointer++;
            if (pointer >= seq->byte_size) {
                printf("Pointer out of bounds while reading literals count.\n");
                return;
            }
        }
        seq->literals_count += input_data[pointer];
        pointer++;

        if (pointer >= seq->byte_size) {
            printf("Pointer out of bounds while reading literals count.\n");
            return;
        }
    }
    // printf("pointer: %d\n", pointer);

    // printf("Match length: %d\n", seq->match_length);
    // printf("Literal length: %d\n", seq->literals_count);

    // Allocate memory for literals
    if (seq->literals_count > 0) {
        seq->literals = (char*)malloc(seq->literals_count * sizeof(char));
        if (seq->literals == NULL) {
            perror("Failed to allocate memory for literals");
            return;
        }
    } else {
        seq->literals = NULL;  // No literals in this sequence
    }

    // Copy literals from input_data
    for (size_t i = 0; i < seq->literals_count; i++) {
        seq->literals[i] = input_data[pointer + i];
    }
    pointer += seq->literals_count;
    // printf("pointer: %d\n", pointer);  // Move the pointer past the literals

    // printf("Literals: ");
    // for (size_t i = 0; i < seq->literals_count; i++) {
    //     if (seq->literals[i] >= 32 && seq->literals[i] <= 126) {
    //         printf("%c", seq->literals[i]);
    //     } else {
    //         printf("0x%02X", (unsigned char)seq->literals[i]);
    //     }
    // }
    // printf("\n");
    // printf("pointer: %d\n", pointer);
    seq->match_offset = (uint16_t)((unsigned char)input_data[pointer] | ((unsigned char)input_data[pointer + 1] << 8));

    pointer += 2;  // Move pointer past the match offset

    //printf("Match offset: %d\n", seq->match_offset);

    // Now handle match length if greater than or equal to 15
    if (seq->match_length >= 15) {
        while (input_data[pointer] == 255) {
            seq->match_length += 255;
            pointer++;  // Move pointer after each byte
            
        }
        seq->match_length += (size_t)(unsigned char)input_data[pointer];
        pointer++;  // Move pointer after the match length byte

        //printf("Adding input_data[pointer] = %X (decimal: %zu), new match_length: %zu\n", 
          //      (unsigned char)input_data[pointer], (size_t)(unsigned char)input_data[pointer], seq->match_length);
    }

    seq->match_length += 4;  // Adjust for the true match length
   // printf("Final Match length: %zu\n", seq->match_length);
}


void parallel_decode_add_block_to_frame(LZ4Frame* frame, LZ4Block block, size_t index) {
    if (frame == NULL) {
        fprintf(stderr, "Error: frame pointer is NULL\n");
        exit(1);
    }


    //printf("index: %zu\n", index);

    frame->frame_blocks[index] = block;
}


DWORD WINAPI parallel_block_decode(LPVOID LpParam) {
    BlockDecodeArgs* args = (BlockDecodeArgs*)LpParam;
    LZ4Block* block = args->block;
    char* input_data = args->blockData;
    block->byte_size = args->block_size;
    

    if (args->blockData == NULL) {
        fprintf(stderr, "Error: blockData is NULL!\n");
        return 0;
    }

    //printf("Input data 0: %02X\n", input_data[0]);
    size_t block_data_size = args->block_size;
    //printf("Block size: %zu\n", block_data_size);

    // Printing input data in chunks of 16 bytes


    block->token = input_data[0];
    size_t pointer = 0;
    //printf("Initial token: %d\n", block->token);

   // printf("Entering loop to process sequences...\n");

    for (size_t i = 0; i < block->token; i++) {
    //printf("Processing sequence %zu, pointer: %zu\n", i, pointer);
    LZ4Sequence seq;


    // Debugging the byte values before calculating seq.byte_size
    //printf("input_data[%zu + 4]: %02X, input_data[%zu + 5]: %02X\n", pointer, input_data[pointer + 4], pointer, input_data[pointer + 5]);

    // Correctly calculate seq.byte_size
    seq.byte_size = (uint16_t)(input_data[pointer + 4]) + ((uint16_t)(input_data[pointer + 5]) << 8);
    //printf("Calculated seq.byte_size: %d\n", seq.byte_size);



    char *seq_data = (char *)malloc(seq.byte_size * sizeof(char));
    if (seq_data == NULL) {
        perror("Failed to allocate memory for seq_data");
        return 1;
    }

    memcpy(seq_data, &input_data[pointer + 3], seq.byte_size);

    EnterCriticalSection(&seq_decode);

    sequence_decode(seq_data, &seq);  // Decode sequence
    add_sequence_to_block(seq, block);  // Update block with sequence data
    
    block->byte_size -= seq.byte_size;
    //printf("Block byte_size after adding sequence: %zu\n", block->byte_size);
    
    LeaveCriticalSection(&seq_decode);
   // printf("After critical section\n");


            pointer += seq.byte_size;
        
        free(seq_data);

}

    //printf("Block byte size before final print: %zu\n", block->byte_size);

   // printf("Calling parallel_add_block_to_frame with arguments:\n");
// printf("Frame pointer: %p\n", args->frame);
// printf("Block byte size: %zu\n", block->byte_size);
// printf("Block token: %d\n", block->sequences_count);

// printf("Block index: %zu\n", args->index);

// Now call the function
parallel_decode_add_block_to_frame(args->frame, *block, args->index);

// printf("Block added succesfully\n");




    free(args);
    free(input_data);

    return 0;
}


char* extract_compressed_bin_file(FILE* file) {
   fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);

    if (file_size == -1) { // Check for ftell failure
        perror("Error getting file size");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    //printf("File size: %zu bytes\n", file_size);

    fseek(file, 0, SEEK_SET);

    char* input = malloc(file_size);

    if (!input) {
        perror("Error allocating memory");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    size_t bytes_read = fread(input, 1, file_size, file);
    //printf("Bytes read: %zu bytes\n", bytes_read);

    if (bytes_read != file_size) {
        fprintf(stderr, "Error: Expected %zu bytes, but only read %zu bytes.\n", file_size, bytes_read);
        free(input);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    return input;
}

void interpret_sequence(LZ4Sequence sequence, uint8_t* decompressed_buffer, size_t* decompressed_size) {
    if (sequence.literals == NULL || sequence.literals_count == 0) {
        // If there are no literals, skip the copying step
        //printf("No literals to process, proceeding to matches.\n");
    }

    size_t current_pos = *decompressed_size;  // Keep track of where to place new decompressed data

    // Ensure no overflow when adding literals and match length
    if (sequence.literals_count + sequence.match_length > SIZE_MAX) {
        printf("Error: decompressed data exceeds size limits\n");
        return;
    }

    // Copy literals to the decompressed buffer
    for (size_t i = 0; i < sequence.literals_count; i++) {
        decompressed_buffer[current_pos++] = sequence.literals[i];
    }

    // // Always process matches (even when literals_count == 0)
     if (sequence.match_offset != 0) {
    //     // Apply matches: reference earlier decompressed data
        //printf("encoding match: ");
        for (size_t i = 0; i < sequence.match_length; i++) {

             size_t match_pos = current_pos - sequence.match_offset;
             //printf("%zu, %zu\n", current_pos, sequence.match_offset);

            // Check if the match_pos is within bounds (it should be a valid reference to already decompressed data)
            if (match_pos < current_pos) {
                decompressed_buffer[current_pos++] = decompressed_buffer[match_pos];
            } else {
                printf("Error: Match offset out of bounds: %zu (match_pos: %zu, current_pos: %zu)\n", 
                       sequence.match_offset, match_pos, current_pos);
                return;
            }
        }
        //printf("\n");
     }

    // Update decompressed size after processing this sequence
    *decompressed_size = current_pos;
}



void interpret_frame(LZ4Frame frame) {
    // Open the uncompressed file for writing (create or overwrite it)
    FILE* uncompressed = safe_open(DEFAULT_UNCOMPRESSED_FILE, "wb");
    fclose(uncompressed);

    // Allocate a buffer to store the entire decompressed data
    size_t decompressed_size = 0; // Initial decompressed size
    uint8_t* decompressed_buffer = malloc(frame.blocks*DEFAULT_BLOCK_LENGTH*2);  // Allocate memory for decompressed data (adjust size as needed)

    if (decompressed_buffer == NULL) {
        printf("Error: Memory allocation failed for decompressed buffer\n");
        return;
    }

    // Process each block in the frame
    for (size_t i = 0; i < frame.blocks; i++) {
        //printf("intepreting block %d\n", i);
        // Process each sequence in the block
        for (size_t j = 0; j < frame.frame_blocks[i].sequences_count; j++) {
            // Pass the buffer and current decompressed size to interpret_sequence
            //printf("intepreting sequence %d\n", j);
            interpret_sequence(frame.frame_blocks[i].sequences[j], decompressed_buffer, &decompressed_size);
        }
    }

    // After all sequences are processed, write the decompressed data to the file
    FILE* uncompressed_file = safe_open(DEFAULT_UNCOMPRESSED_FILE, "wb");
    if (uncompressed_file == NULL) {
        printf("Error: Unable to open uncompressed file for writing\n");
        free(decompressed_buffer);
        return;
    }

    // Write decompressed data to the file
    for (size_t i = 0; i < decompressed_size; i++) {
        // Write as text if printable, otherwise in hex format
        if (decompressed_buffer[i] >= 32 && decompressed_buffer[i] <= 126) {
            fprintf(uncompressed_file, "%c", decompressed_buffer[i]);
        } else {
            fprintf(uncompressed_file, "0x%02X", decompressed_buffer[i]);
        }
    }

    fclose(uncompressed_file);
    free(decompressed_buffer);  // Free the buffer after use
}



void parallel_LZ4_decode(char* input_bin_file, char* log) {
    if (DEFAULT_BLOCK_LENGTH == 500) {
        printf("Error: block length cannot have the value 500");
        exit(1);
    }

    //ensure_directories();

    FILE* input_file = safe_open(input_bin_file, "rb");
    FILE* log_file = safe_open(log, "a");

    LZ4Frame frame_decode = {0};
    char* input = extract_compressed_bin_file(input_file);
    size_t file_size = ftell(input_file); // The file size from the input file (same as extracted file_size)

    size_t block_count = input[0]; // First byte indicates block count
    size_t pointer = 1; // Start reading after the block count

    frame_decode.frame_blocks = malloc(block_count * sizeof(LZ4Block));
    frame_decode.blocks = 0;

    InitializeCriticalSection(&block_cs);

    HANDLE* threads = malloc(block_count * sizeof(HANDLE));
    DWORD* threads_IDs = malloc(block_count * sizeof(DWORD));
    CHECK_ALLOCATION(threads);
    CHECK_ALLOCATION(threads_IDs);

    for (size_t i = 0; i < block_count; i++) {
        LZ4Block block = {0};

        uint8_t byte1 = (uint8_t)input[pointer + 1];
        uint8_t byte2 = (uint8_t)input[pointer + 2];
        size_t byte_size = byte1 + ((size_t)byte2 << 8); // Combine two bytes to get the block size

        if (byte_size <= 0) {
            printf("Error: invalid block size at block %zu\n", i);
            exit(EXIT_FAILURE);
        }

        // Ensure that the pointer + byte_size does not exceed the input array bounds
        if (pointer + byte_size > file_size) {
            printf("Pointer + byte_size exceeds input array bounds\n");
            break; // Handle the case when the block exceeds bounds
        }

        // Allocate memory for BlockDecodeArgs and block data
        BlockDecodeArgs* args = (BlockDecodeArgs*)malloc(sizeof(BlockDecodeArgs));
        if (args == NULL) {
            printf("Memory allocation failed for BlockDecodeArgs\n");
            exit(1);
        }
        memset(args, 0, sizeof(BlockDecodeArgs));

        // Allocate memory for block data
        args->blockData = (char*)malloc(byte_size * sizeof(uint8_t));
        if (args->blockData == NULL) {
            printf("Memory allocation failed for blockData\n");
            free(args);
            exit(1);
        }

        // Copy the block data from input into blockData
        memcpy(args->blockData, &input[pointer], byte_size);

        // Assign block data to args
        args->block = &block;
        args->block_size = byte_size;
        args->frame = &frame_decode;
        args->index=i;

        // Create a thread to decode the block
        threads[i] = CreateThread(NULL, 0, parallel_block_decode, args, 0, &threads_IDs[i]);
        WaitForSingleObject(threads[i],INFINITE);
        if (threads[i] == NULL) {
            fprintf(stderr, "Error: Failed to create thread for block %zu\n", i);
            exit(1);
        }


        // Update pointer for the next block
        pointer += byte_size;
        //printf("block done");

            }
            //printf("frame done");

    // Wait for all threads to finish
    WaitForMultipleObjects(block_count, threads, TRUE, INFINITE);
    //printf("parallel done");

    // Clean up thread resources
    for (size_t i = 0; i < block_count; i++) {
        CloseHandle(threads[i]);
    }

    // Free allocated memory
    free(threads);
    free(threads_IDs);

    frame_decode.blocks=block_count;
    // Delete critical section
    DeleteCriticalSection(&block_cs);

    // Print details and process the decoded frame
    //print_frame_details(log_file, &frame_decode);

    // Free the input buffer and decode the frame
    free(input);
    interpret_frame(frame_decode);

    // Free the frame resources
    free_frame(&frame_decode);

    // Close files
    fclose(input_file);
    fclose(log_file);
}


#pragma endregion LZ4_API

int main() {
    //ensure_directories();
    InitializeCriticalSection(&block_plus);
    InitializeCriticalSection(&add_seq);
    InitializeCriticalSection(&seq_decode);
    clear_files();

    parallel_LZ4_encode();

    parallel_LZ4_decode(DEFAULT_COMPRESSED_FILE, DEFAULT_LOG_FILE);

    DeleteCriticalSection(&block_plus);
    DeleteCriticalSection(&add_seq);
    DeleteCriticalSection(&seq_decode);

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    int num_cores = sysInfo.dwNumberOfProcessors;  // Number of cores (logical processors)
    printf("Number of cores available: %d\n", num_cores);

    return 0;
    return 0;
}