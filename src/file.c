#include "file.h"

char *read_file(const char *filename) {
    FILE *file = fopen(filename, "rb");  // Open in binary mode
    if (!file) {
        perror("fopen");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);  // Get file size
    if (length < 0) {
        perror("ftell");
        fclose(file);
        return NULL;
    }
    rewind(file);

    char *buffer = malloc(length + 1);
    if (!buffer) {
        perror("malloc");
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, length, file) != (size_t)length) {
        perror("fread");
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[length] = '\0';  // Null-terminate the string
    fclose(file);
    return buffer;
}

void free_file_content(char *content) {
    if (content) {
        free(content);
    }
}