#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Structure to hold a dictionary entry with an index and pattern
struct Tuple {
    int index;
    char* pattern;
};

// Log file pointer
FILE* logFile;

// Function to log messages to a file
void logMessage(const char* message) {
    fprintf(logFile, "%s\n", message);
}

// Function to initialize the dictionary with the base alphabet (ASCII characters)
//TODO: dynamically change the size of basealphabet
struct Tuple* buildAlphabet(char baseAlphabet[]) {
    struct Tuple* dic = (struct Tuple*)malloc(128 * sizeof(struct Tuple));  // Dynamically allocate memory for the dictionary
    if (dic == NULL) {
        logMessage("Memory allocation failed for dictionary");
        return NULL;  // Return NULL if allocation fails
    }

    // Step 1: Initialize the dictionary with the base alphabet
    for (int i = 0; i < 128; i++) {
        struct Tuple pair;
        pair.index = i;

        // Allocate memory for pattern (2 bytes to hold the character and null-terminator)
        pair.pattern = (char*)malloc(2 * sizeof(char)); 
        if (pair.pattern == NULL) {
            logMessage("Memory allocation failed for pattern");
            free(dic);
            return NULL;  // Return early if allocation fails
        }

        // Assign the character and null-terminate the string
        pair.pattern[0] = baseAlphabet[i];
        pair.pattern[1] = '\0';  // Null-terminate the string

        dic[i] = pair; // Store the pair in the dictionary
    }

    logMessage("Initialized dictionary with base alphabet");
    return dic;
}

// Function to create a new dictionary entry
struct Tuple createEntry(int index, char* pattern){
    struct Tuple pair;
    pair.index = index;
    pair.pattern = pattern;
    return pair;
}

// Function to add a new entry to the dictionary (resize the dictionary if needed)
// Function to add a new entry to the dictionary (resize the dictionary if needed)
struct Tuple* addNewEntry(struct Tuple* oldDic, char* pattern, int* length) {
    // Try to resize the dictionary
    struct Tuple* temp = realloc(oldDic, (*length + 1) * sizeof(struct Tuple));
    if (temp == NULL) {
        logMessage("Memory reallocation failed");
        return oldDic;  // Return original dictionary if reallocation fails
    }

    // Now we can safely update the dictionary
    oldDic = temp;

    // Allocate memory for the new pattern string
    oldDic[*length].pattern = (char*)malloc((strlen(pattern) + 1) * sizeof(char));  // +1 for null-terminator
    if (oldDic[*length].pattern == NULL) {
        logMessage("Memory allocation failed for new pattern");
        return oldDic;  // Return original dictionary if memory allocation fails
    }

    // Copy the new pattern to the allocated memory
    strcpy(oldDic[*length].pattern, pattern);
    oldDic[*length].index = *length;  // Set the index of the new pattern

    // Increment the dictionary length
    (*length)++;

    logMessage("Added new pattern to dictionary");
    return oldDic;
}

int dictionarySearch(struct Tuple* dictionary, char pattern[], int dictionaryLength){
    for(int i = 0; i < dictionaryLength; i++) {
        if(strcmp(dictionary[i].pattern, pattern) == 0){
            return dictionary[i].index;
        }
    }
    return -1;
}

void concatenateEncodedValue(char **output, int *outputSize, int encodedValue) {
    // Prepare a temporary string to hold the encoded value as a string
    char temp[50];  // Make sure this is large enough to hold the integer as a string
    snprintf(temp, sizeof(temp), "%d ", encodedValue);

    // Calculate the new size needed for the output string
    int newLength = strlen(*output) + strlen(temp) + 1;  // +1 for the null-terminator

    // Reallocate memory for the output string if necessary
    if (newLength > *outputSize) {
        *outputSize = newLength + 128;  // Add extra space for future concatenation
        *output = realloc(*output, *outputSize);
        if (*output == NULL) {
            printf("Memory reallocation failed!\n");
            exit(1);  // Exit if realloc fails
        }
    }

    // Concatenate the new encoded value string to the output
    strcat(*output, temp);
}

void printDictionary(struct Tuple* dictionary, int dictionaryLength) {
    // Print the table header
    printf("Index\tPattern\n");
    printf("--------------------\n");

    // Iterate through the dictionary and print each entry
    for (int i = 0; i < dictionaryLength; i++) {
        printf("%d\t%s\n", dictionary[i].index, dictionary[i].pattern);
    }
}



char* LZW_Encode(char source[], char baseAlphabet[]) {
    // Initialize the dictionary with the base alphabet (ASCII characters)
    struct Tuple* dictionary = buildAlphabet(baseAlphabet);
    int dictionaryLength = 128;  // Start with the base alphabet size
    int outputSize = 128;  
    char *output = (char*)malloc(outputSize * sizeof(char));
    if (output == NULL) {
        printf("Memory allocation failed!\n");
        return NULL;
    }

    // Start with an empty string
    output[0] = '\0';

    if (dictionary == NULL) {
        return NULL;  
    }

    // Initialize current string (W) to the first symbol in the input.
    char* w = (char*)malloc(256 * sizeof(char));  // TODO: Dynamically allocate space for w
    w[0] = '\0';  // Start with an empty string
    char c;
    int pointerIndex = 0;  // Start at the beginning of the source string
    logMessage("Starting encoding");

    // While there are symbols left in the input:
    while (source[pointerIndex] != '\0') {
        // Read the next symbol (C) and append it to W.
        char wPrime[256];
        strcpy(wPrime, w);  // Copy the current string (W) into wPrime
        c = source[pointerIndex];  // Next character
        strncat(wPrime, &c, 1);  // Append C to W'
        
        logMessage("Analyzing sequence");
        logMessage(wPrime);
        pointerIndex++;

        // If W' (wPrime) is in the dictionary:
        int foundIndex = dictionarySearch(dictionary, wPrime, dictionaryLength);
        if (foundIndex != -1) {
            // Continue processing by reading the next symbol, so we extend W.
            logMessage("Pattern found in dictionary, continuing with extended sequence");
            strcpy(w, wPrime);  // Update W with W'
        } else {
            // If W' is not in the dictionary:
            // Output the index of W (from the dictionary).
            int encoded = dictionarySearch(dictionary, w, dictionaryLength);
            logMessage("New Pattern found!");
            logMessage("Pattern encoded:");
            logMessage(w);
            logMessage("Encoded index:");
            fprintf(logFile, "%d\n", encoded);  // Output the index of the last W
            concatenateEncodedValue(&output, &outputSize, encoded);

            // Add the string WC (W + C) to the dictionary.
            dictionary = addNewEntry(dictionary, wPrime, &dictionaryLength);
            logMessage("New pattern added to dictionary");

            // Set W to the next character C and continue processing.
            w[0] = c;
            w[1] = '\0';  // Ensure it's null-terminated
        }
    }

    // Output any remaining sequence in W.
    if (w[0] != '\0') {
        int encoded = dictionarySearch(dictionary, w, dictionaryLength);
        concatenateEncodedValue(&output, &outputSize, encoded);
        logMessage("Remaining pattern encoded:");
        logMessage(w);
        logMessage("Encoded index:");
        fprintf(logFile, "%d\n", encoded);  // Output the index of the last W
    }

    logMessage("Encoding complete");

    free(w); // free the memory allocated for w

    // Free the memory allocated for each pattern in the dictionary
    for (int i = 0; i < dictionaryLength; i++) {
        free(dictionary[i].pattern);  // Free memory allocated for the pattern strings
    }
    printDictionary(dictionary, dictionaryLength);
    free(dictionary); // Finally, free the dictionary itself

    return output;
}

int main() {
    // Example input string to be encoded
    char text[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nam eu pharetra eros. In tincidunt rhoncus ligula sit amet lacinia. Pellentesque molestie auctor nunc, ut egestas ex rhoncus eget. Donec faucibus hendrerit augue quis ornare. Etiam sapien erat, pulvinar vitae ultrices non, dapibus a tortor. Duis interdum ornare purus. Aenean ornare, orci sit amet elementum sollicitudin, turpis odio ultrices ipsum, vel condimentum neque purus eget dui. Class aptent taciti sociosqu ad litora torquent per conubia nostra, per inceptos himenaeos. Nam aliquam congue posuere. Sed suscipit massa at accumsan venenatis. Aenean faucibus, nulla sit amet sodales porta, tellus tortor luctus lectus, quis vulputate velit lectus in lorem";

    // Base alphabet (ASCII characters)
    char asciiDictionary[128] = {
        '\0', '\1', '\2', '\3', '\4', '\5', '\6', '\7', '\b', '\t', '\n', '\v', '\f', '\r', ' ', '!', 
        '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/', '0', '1', '2', '3', 
        '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?', '@', 'A', 'B', 'C', 'D', 'E', 'F', 
        'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 
        'Z', '[', '\\', ']', '^', '_', '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 
        'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', '\177'
    };

    // Open log file for writing debug logs
    logFile = fopen("encoding_log.txt", "w");
    if (logFile == NULL) {
        printf("Unable to open log file\n");
        return -1;
    }

    // Call LZW encoding function
    char* output = LZW_Encode(text, asciiDictionary);
    logMessage("Final output:");
    fprintf(logFile, "%s", output);

    // Close the log file
    fclose(logFile);

    printf("Encoding completed. Check 'encoding_log.txt' for details.\n");

    return 0;
}
