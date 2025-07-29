#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "vm.h"

#define MAX_LABELS 100
#define MAX_LINE_LEN 1024

typedef struct {
    char name[64];
    uint16_t address;
} label_t;

typedef struct {
    char name[64];
    uint16_t address;
    uint16_t patch_location;
} label_ref_t;

static label_t labels[MAX_LABELS];
static label_ref_t label_refs[MAX_LABELS];
static int label_count = 0;
static int label_ref_count = 0;
static uint8_t output[VM_MEMORY_SIZE];
static uint16_t output_pos = 0;

void emit_byte(uint8_t byte) {
    if (output_pos < VM_MEMORY_SIZE) {
        output[output_pos++] = byte;
    }
}

void emit_word(uint16_t word) {
    emit_byte(word & 0xFF);        
    emit_byte((word >> 8) & 0xFF);
}

int find_label(const char* name) {
    for (int i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void add_label(const char* name, uint16_t address) {
    if (label_count < MAX_LABELS) {
        strcpy(labels[label_count].name, name);
        labels[label_count].address = address;
        label_count++;
    }
}

void add_label_ref(const char* name, uint16_t patch_location) {
    if (label_ref_count < MAX_LABELS) {
        strcpy(label_refs[label_ref_count].name, name);
        label_refs[label_ref_count].patch_location = patch_location;
        label_ref_count++;
    }
}

void patch_labels() {
    for (int i = 0; i < label_ref_count; i++) {
        int label_idx = find_label(label_refs[i].name);
        if (label_idx >= 0) {
            uint16_t addr = labels[label_idx].address;
            uint16_t patch_pos = label_refs[i].patch_location;
            output[patch_pos] = addr & 0xFF;
            output[patch_pos + 1] = (addr >> 8) & 0xFF;
        } else {
            printf("Error: Undefined label '%s'\n", label_refs[i].name);
        }
    }
}

int parse_number(const char* str) {
    if (str[0] == '0' && str[1] == 'x') {
        return (int)strtol(str, NULL, 16);
    }
    return (int)strtol(str, NULL, 10);
}

void trim_whitespace(char* str) {
    
    char* start = str;
    while (isspace(*start)) start++;
    
    
    char* end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) end--;
    
    
    size_t len = end - start + 1;
    memmove(str, start, len);
    str[len] = '\0';
}

int assemble_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Cannot open file '%s'\n", filename);
        return 1;
    }
    
    char line[MAX_LINE_LEN];
    int line_num = 0;
    
    
    while (fgets(line, sizeof(line), file)) {
        line_num++;
        trim_whitespace(line);
        
        
        if (line[0] == '\0' || line[0] == ';') continue;
        
        
        char* colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            trim_whitespace(line);
            add_label(line, output_pos);
            
            
            char* instruction = colon + 1;
            trim_whitespace(instruction);
            if (instruction[0] != '\0' && instruction[0] != ';') {
                strcpy(line, instruction);
            } else {
                continue;
            }
        }
        
        
        char* token = strtok(line, " \t");
        if (!token) continue;
        
        
        for (char* p = token; *p; p++) *p = toupper(*p);
        
        if (strcmp(token, "NOP") == 0) {
            emit_byte(OP_NOP);
        } else if (strcmp(token, "HALT") == 0) {
            emit_byte(OP_HALT);
        } else if (strcmp(token, "PUSH") == 0) {
            emit_byte(OP_PUSH);
            token = strtok(NULL, " \t");
            if (token) {
                emit_byte(parse_number(token));
            }
        } else if (strcmp(token, "POP") == 0) {
            emit_byte(OP_POP);
        } else if (strcmp(token, "DUP") == 0) {
            emit_byte(OP_DUP);
        } else if (strcmp(token, "SWAP") == 0) {
            emit_byte(OP_SWAP);
        } else if (strcmp(token, "ADD") == 0) {
            emit_byte(OP_ADD);
        } else if (strcmp(token, "SUB") == 0) {
            emit_byte(OP_SUB);
        } else if (strcmp(token, "MUL") == 0) {
            emit_byte(OP_MUL);
        } else if (strcmp(token, "DIV") == 0) {
            emit_byte(OP_DIV);
        } else if (strcmp(token, "MOD") == 0) {
            emit_byte(OP_MOD);
        } else if (strcmp(token, "NEG") == 0) {
            emit_byte(OP_NEG);
        } else if (strcmp(token, "AND") == 0) {
            emit_byte(OP_AND);
        } else if (strcmp(token, "OR") == 0) {
            emit_byte(OP_OR);
        } else if (strcmp(token, "XOR") == 0) {
            emit_byte(OP_XOR);
        } else if (strcmp(token, "NOT") == 0) {
            emit_byte(OP_NOT);
        } else if (strcmp(token, "SHL") == 0) {
            emit_byte(OP_SHL);
        } else if (strcmp(token, "SHR") == 0) {
            emit_byte(OP_SHR);
        } else if (strcmp(token, "EQ") == 0) {
            emit_byte(OP_EQ);
        } else if (strcmp(token, "NEQ") == 0) {
            emit_byte(OP_NEQ);
        } else if (strcmp(token, "GT") == 0) {
            emit_byte(OP_GT);
        } else if (strcmp(token, "LT") == 0) {
            emit_byte(OP_LT);
        } else if (strcmp(token, "GTE") == 0) {
            emit_byte(OP_GTE);
        } else if (strcmp(token, "LTE") == 0) {
            emit_byte(OP_LTE);
        } else if (strcmp(token, "LOAD") == 0) {
            emit_byte(OP_LOAD);
            token = strtok(NULL, " \t");
            if (token) {
                if (isalpha(token[0])) {
                    add_label_ref(token, output_pos);
                    emit_word(0); 
                } else {
                    emit_word(parse_number(token));
                }
            }
        } else if (strcmp(token, "STORE") == 0) {
            emit_byte(OP_STORE);
            token = strtok(NULL, " \t");
            if (token) {
                if (isalpha(token[0])) {
                    add_label_ref(token, output_pos);
                    emit_word(0); 
                } else {
                    emit_word(parse_number(token));
                }
            }
        } else if (strcmp(token, "LOAD_IND") == 0) {
            emit_byte(OP_LOAD_IND);
        } else if (strcmp(token, "STORE_IND") == 0) {
            emit_byte(OP_STORE_IND);
        } else if (strcmp(token, "JMP") == 0) {
            emit_byte(OP_JMP);
            token = strtok(NULL, " \t");
            if (token) {
                if (isalpha(token[0])) {
                    add_label_ref(token, output_pos);
                    emit_word(0); 
                } else {
                    emit_word(parse_number(token));
                }
            }
        } else if (strcmp(token, "JZ") == 0) {
            emit_byte(OP_JZ);
            token = strtok(NULL, " \t");
            if (token) {
                if (isalpha(token[0])) {
                    add_label_ref(token, output_pos);
                    emit_word(0); 
                } else {
                    emit_word(parse_number(token));
                }
            }
        } else if (strcmp(token, "JNZ") == 0) {
            emit_byte(OP_JNZ);
            token = strtok(NULL, " \t");
            if (token) {
                if (isalpha(token[0])) {
                    add_label_ref(token, output_pos);
                    emit_word(0); 
                } else {
                    emit_word(parse_number(token));
                }
            }
        } else if (strcmp(token, "CALL") == 0) {
            emit_byte(OP_CALL);
            token = strtok(NULL, " \t");
            if (token) {
                if (isalpha(token[0])) {
                    add_label_ref(token, output_pos);
                    emit_word(0); 
                } else {
                    emit_word(parse_number(token));
                }
            }
        } else if (strcmp(token, "RET") == 0) {
            emit_byte(OP_RET);
        } else if (strcmp(token, "SYS") == 0) {
            emit_byte(OP_IO);
            token = strtok(NULL, " \t");
            if (token) {
                emit_byte(parse_number(token));
            }
        } else {
            printf("Warning: Unknown instruction '%s' at line %d\n", token, line_num);
        }
    }
    
    fclose(file);
    
    patch_labels();
    
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input.asm> <output.bin>\n", argv[0]);
        return 1;
    }
    
    if (assemble_file(argv[1]) != 0) {
        return 1;
    }
    
    FILE* output_file = fopen(argv[2], "wb");
    if (!output_file) {
        printf("Error: Cannot create output file '%s'\n", argv[2]);
        return 1;
    }
    
    fwrite(output, 1, output_pos, output_file);
    fclose(output_file);
    
    printf("Assembly complete: %d bytes written to '%s'\n", output_pos, argv[2]);
    return 0;
}
