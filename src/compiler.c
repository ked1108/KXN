#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_TOKENS 10000
#define MAX_SYMBOLS 256
#define MAX_CODE_LINES 10000
#define MAX_IDENTIFIER_LEN 64
#define MAX_LINE_LEN 1024

#define SYS_EXIT        0x00
#define SYS_PRINT_CHAR  0x01
#define SYS_READ_CHAR   0x02
#define SYS_DRAW_PIXEL  0x10
#define SYS_DRAW_LINE   0x11
#define SYS_FILL_RECT   0x12
#define SYS_REFRESH     0x13
#define SYS_POLL_KEY    0x20
#define SYS_GET_KEY     0x21

#define VAR_START_ADDR 0x0100

// Token types
typedef enum {
	TOKEN_EOF = 0,
	TOKEN_IDENTIFIER,
	TOKEN_NUMBER,
	TOKEN_VAR,
	TOKEN_IF,
	TOKEN_ELSE,
	TOKEN_WHILE,
	TOKEN_RETURN,
	TOKEN_ASSIGN,        // =
	TOKEN_PLUS,          // +
	TOKEN_MINUS,         // -
	TOKEN_MULTIPLY,      // *
	TOKEN_DIVIDE,        // /
	TOKEN_EQUALS,        // ==
	TOKEN_NOT_EQUALS,    // !=
	TOKEN_LESS,          // <
	TOKEN_GREATER,       // >
	TOKEN_LESS_EQUAL,    // <=
	TOKEN_GREATER_EQUAL, // >=
	TOKEN_SEMICOLON,     // ;
	TOKEN_COMMA,         // ,
	TOKEN_LPAREN,        // (
	TOKEN_RPAREN,        // )
	TOKEN_LBRACE,        // {
	TOKEN_RBRACE,        // }
	TOKEN_UNKNOWN
} token_type_t;

typedef struct {
	token_type_t type;
	char value[MAX_IDENTIFIER_LEN];
	int line;
	int column;
} token_t;

typedef struct {
	char name[MAX_IDENTIFIER_LEN];
	uint16_t address;
	bool initialized;
} symbol_t;

typedef struct {
	token_t tokens[MAX_TOKENS];
	int token_count;
	int current_token;

	symbol_t symbols[MAX_SYMBOLS];
	int symbol_count;
	uint16_t next_var_addr;

	char output[MAX_CODE_LINES][MAX_LINE_LEN];
	int output_count;

	int label_counter;
	int current_line;
} compiler_t;

void error(compiler_t* comp, const char* message);
void emit(compiler_t* comp, const char* code);
void emit_label(compiler_t* comp, const char* label);
char* new_label(compiler_t* comp);
token_t* current_token(compiler_t* comp);
token_t* peek_token(compiler_t* comp);
void consume_token(compiler_t* comp);
bool match_token(compiler_t* comp, token_type_t type);
void expect_token(compiler_t* comp, token_type_t type);

bool is_keyword(const char* str, token_type_t* type);
void tokenize(compiler_t* comp, const char* source);

void parse_multiplicative(compiler_t* comp);
void parse_statement(compiler_t* comp);
void parse_var_declaration(compiler_t* comp);
void parse_assignment(compiler_t* comp);
void parse_if_statement(compiler_t* comp);
void parse_while_statement(compiler_t* comp);
void parse_expression_statement(compiler_t* comp);
void parse_expression(compiler_t* comp);
void parse_comparison(compiler_t* comp);
void parse_term(compiler_t* comp);
void parse_factor(compiler_t* comp);
void parse_function_call(compiler_t* comp, const char* func_name);

symbol_t* find_symbol(compiler_t* comp, const char* name);
symbol_t* add_symbol(compiler_t* comp, const char* name);

bool is_builtin_function(const char* name);
int get_builtin_syscall(const char* name);

void error(compiler_t* comp, const char* message) {
	token_t* tok = current_token(comp);
	fprintf(stderr, "Error at line %d: %s\n", tok->line, message);
	exit(1);
}

void emit(compiler_t* comp, const char* code) {
	if (comp->output_count >= MAX_CODE_LINES) {
		error(comp, "Too many output lines - program too complex");
	}
	if (strlen(code) >= MAX_LINE_LEN) {
		error(comp, "Output line too long");
	}
	strcpy(comp->output[comp->output_count++], code);
}

void emit_label(compiler_t* comp, const char* label) {
	char line[MAX_LINE_LEN];
	snprintf(line, sizeof(line), "%s:", label);
	emit(comp, line);
}

char* new_label(compiler_t* comp) {
	static char labels[10][32];
	static int current_label_slot = 0;
	
	char* label = labels[current_label_slot];
	snprintf(label, 32, "L%d", comp->label_counter++);
	
	current_label_slot = (current_label_slot + 1) % 10;
	return label;
}

token_t* current_token(compiler_t* comp) {
	if (comp->current_token >= comp->token_count) {
		static token_t eof_token = {TOKEN_EOF, "", 0, 0};
		return &eof_token;
	}
	return &comp->tokens[comp->current_token];
}

token_t* peek_token(compiler_t* comp) {
	if (comp->current_token + 1 >= comp->token_count) {
		static token_t eof_token = {TOKEN_EOF, "", 0, 0};
		return &eof_token;
	}
	return &comp->tokens[comp->current_token + 1];
}

void consume_token(compiler_t* comp) {
	if (comp->current_token < comp->token_count) {
		comp->current_token++;
	}
}

bool match_token(compiler_t* comp, token_type_t type) {
	if (current_token(comp)->type == type) {
		consume_token(comp);
		return true;
	}
	return false;
}

void expect_token(compiler_t* comp, token_type_t type) {
	if (current_token(comp)->type != type) {
		char error_msg[MAX_LINE_LEN];
		snprintf(error_msg, sizeof(error_msg), "Expected token type %d, got %d", type, current_token(comp)->type);
		error(comp, error_msg);
	}
	consume_token(comp);
}

bool is_keyword(const char* str, token_type_t* type) {
	if (strcmp(str, "var") == 0) { *type = TOKEN_VAR; return true; }
	if (strcmp(str, "if") == 0) { *type = TOKEN_IF; return true; }
	if (strcmp(str, "else") == 0) { *type = TOKEN_ELSE; return true; }
	if (strcmp(str, "while") == 0) { *type = TOKEN_WHILE; return true; }
	if (strcmp(str, "return") == 0) { *type = TOKEN_RETURN; return true; }
	return false;
}

void tokenize(compiler_t* comp, const char* source) {
	const char* p = source;
	int line = 1;
	int column = 1;

	comp->token_count = 0;

	while (*p) {
		while (isspace(*p)) {
			if (*p == '\n') {
				line++;
				column = 1;
			} else {
				column++;
			}
			p++;
		}

		if (!*p) break;

		token_t* token = &comp->tokens[comp->token_count];
		token->line = line;
		token->column = column;

		if (*p == '/' && *(p + 1) == '/') {
			while (*p && *p != '\n') p++;
			continue;
		}

		if (isdigit(*p)) {
			int i = 0;
			while (isdigit(*p) && i < MAX_IDENTIFIER_LEN - 1) {
				token->value[i++] = *p++;
				column++;
			}
			token->value[i] = '\0';
			token->type = TOKEN_NUMBER;
		}
		else if (isalpha(*p) || *p == '_') {
			int i = 0;
			while ((isalnum(*p) || *p == '_') && i < MAX_IDENTIFIER_LEN - 1) {
				token->value[i++] = *p++;
				column++;
			}
			token->value[i] = '\0';

			if (!is_keyword(token->value, &token->type)) {
				token->type = TOKEN_IDENTIFIER;
			}
		}
		else if (*p == '=' && *(p + 1) == '=') {
			strcpy(token->value, "==");
			token->type = TOKEN_EQUALS;
			p += 2;
			column += 2;
		}
		else if (*p == '!' && *(p + 1) == '=') {
			strcpy(token->value, "!=");
			token->type = TOKEN_NOT_EQUALS;
			p += 2;
			column += 2;
		}
		else if (*p == '<' && *(p + 1) == '=') {
			strcpy(token->value, "<=");
			token->type = TOKEN_LESS_EQUAL;
			p += 2;
			column += 2;
		}
		else if (*p == '>' && *(p + 1) == '=') {
			strcpy(token->value, ">=");
			token->type = TOKEN_GREATER_EQUAL;
			p += 2;
			column += 2;
		}
		else {
			token->value[0] = *p;
			token->value[1] = '\0';

			switch (*p) {
				case '=': token->type = TOKEN_ASSIGN; break;
				case '+': token->type = TOKEN_PLUS; break;
				case '-': token->type = TOKEN_MINUS; break;
				case '*': token->type = TOKEN_MULTIPLY; break;
				case '/': token->type = TOKEN_DIVIDE; break;
				case '<': token->type = TOKEN_LESS; break;
				case '>': token->type = TOKEN_GREATER; break;
				case ';': token->type = TOKEN_SEMICOLON; break;
				case ',': token->type = TOKEN_COMMA; break;
				case '(': token->type = TOKEN_LPAREN; break;
				case ')': token->type = TOKEN_RPAREN; break;
				case '{': token->type = TOKEN_LBRACE; break;
				case '}': token->type = TOKEN_RBRACE; break;
				default: token->type = TOKEN_UNKNOWN; break;
			}
			p++;
			column++;
		}

		comp->token_count++;
		if (comp->token_count >= MAX_TOKENS) {
			error(comp, "Too many tokens");
		}
	}
}

symbol_t* find_symbol(compiler_t* comp, const char* name) {
	for (int i = 0; i < comp->symbol_count; i++) {
		if (strcmp(comp->symbols[i].name, name) == 0) {
			return &comp->symbols[i];
		}
	}
	return NULL;
}

symbol_t* add_symbol(compiler_t* comp, const char* name) {
	if (comp->symbol_count >= MAX_SYMBOLS) {
		error(comp, "Too many symbols - reduce number of variables");
	}

	if (strlen(name) >= MAX_IDENTIFIER_LEN) {
		error(comp, "Variable name too long");
	}

	symbol_t* sym = &comp->symbols[comp->symbol_count++];
	strcpy(sym->name, name);
	sym->address = comp->next_var_addr++;
	sym->initialized = false;
	return sym;
}

bool is_builtin_function(const char* name) {
	return (strcmp(name, "draw_pixel") == 0 ||
	strcmp(name, "draw_line") == 0 ||
	strcmp(name, "fill_rect") == 0 ||
	strcmp(name, "refresh") == 0 ||
	strcmp(name, "print_char") == 0 ||
	strcmp(name, "read_char") == 0 ||
	strcmp(name, "halt") == 0);
}

int get_builtin_syscall(const char* name) {
	if (strcmp(name, "draw_pixel") == 0) return SYS_DRAW_PIXEL;
	if (strcmp(name, "draw_line") == 0) return SYS_DRAW_LINE;
	if (strcmp(name, "fill_rect") == 0) return SYS_FILL_RECT;
	if (strcmp(name, "refresh") == 0) return SYS_REFRESH;
	if (strcmp(name, "print_char") == 0) return SYS_PRINT_CHAR;
	if (strcmp(name, "read_char") == 0) return SYS_READ_CHAR;
	if (strcmp(name, "halt") == 0) return SYS_EXIT;
	return -1;
}

void parse_program(compiler_t* comp) {
	while (current_token(comp)->type != TOKEN_EOF) {
		parse_statement(comp);
	}

	if (comp->output_count == 0 || 
		strstr(comp->output[comp->output_count - 1], "HALT") == NULL) {
		emit(comp, "HALT");
	}
}

void parse_statement(compiler_t* comp) {
	token_t* tok = current_token(comp);

	switch (tok->type) {
		case TOKEN_VAR:
			parse_var_declaration(comp);
			break;
		case TOKEN_IF:
			parse_if_statement(comp);
			break;
		case TOKEN_WHILE:
			parse_while_statement(comp);
			break;
		case TOKEN_IDENTIFIER:
			if (peek_token(comp)->type == TOKEN_ASSIGN) {
				parse_assignment(comp);
			} else {
				parse_expression_statement(comp);
			}
			break;
		case TOKEN_LBRACE:
			consume_token(comp); 
			while (current_token(comp)->type != TOKEN_RBRACE && 
				current_token(comp)->type != TOKEN_EOF) {
				parse_statement(comp);
			}
			expect_token(comp, TOKEN_RBRACE);
			break;
		default:
			parse_expression_statement(comp);
			break;
	}
}

void parse_var_declaration(compiler_t* comp) {
	expect_token(comp, TOKEN_VAR);

	if (current_token(comp)->type != TOKEN_IDENTIFIER) {
		error(comp, "Expected variable name");
	}

	char var_name[MAX_IDENTIFIER_LEN];
	strcpy(var_name, current_token(comp)->value);
	consume_token(comp);

	symbol_t* sym = find_symbol(comp, var_name);
	if (sym) {
		error(comp, "Variable already declared");
	}

	sym = add_symbol(comp, var_name);

	if (match_token(comp, TOKEN_ASSIGN)) {
		parse_expression(comp);
		char line[MAX_LINE_LEN];
		snprintf(line, sizeof(line), "STORE 0x%04X", sym->address);
		emit(comp, line);
		sym->initialized = true;
	}

	expect_token(comp, TOKEN_SEMICOLON);
}

void parse_assignment(compiler_t* comp) {
	if (current_token(comp)->type != TOKEN_IDENTIFIER) {
		error(comp, "Expected variable name");
	}

	char var_name[MAX_IDENTIFIER_LEN];
	strcpy(var_name, current_token(comp)->value);
	consume_token(comp);

	symbol_t* sym = find_symbol(comp, var_name);
	if (!sym) {
		error(comp, "Undefined variable");
	}

	expect_token(comp, TOKEN_ASSIGN);
	parse_expression(comp);

	char line[MAX_LINE_LEN];
	snprintf(line, sizeof(line), "STORE 0x%04X", sym->address);
	emit(comp, line);
	sym->initialized = true;

	expect_token(comp, TOKEN_SEMICOLON);
}

void parse_if_statement(compiler_t* comp) {
	expect_token(comp, TOKEN_IF);
	expect_token(comp, TOKEN_LPAREN);

	parse_expression(comp);

	expect_token(comp, TOKEN_RPAREN);

	
	int if_id = comp->label_counter;
	char else_label[32], end_label[32];
	snprintf(else_label, sizeof(else_label), "IF_ELSE_%d", if_id);
	snprintf(end_label, sizeof(end_label), "IF_END_%d", if_id);
	comp->label_counter += 2; 

	char line[MAX_LINE_LEN];
	snprintf(line, sizeof(line), "JZ %s", else_label);
	emit(comp, line);

	parse_statement(comp);

	if (current_token(comp)->type == TOKEN_ELSE) {
		consume_token(comp);
		snprintf(line, sizeof(line), "JMP %s", end_label);
		emit(comp, line);
		emit_label(comp, else_label);
		parse_statement(comp);
		emit_label(comp, end_label);
	} else {
		emit_label(comp, else_label);
	}
}

void parse_while_statement(compiler_t* comp) {
	expect_token(comp, TOKEN_WHILE);
	expect_token(comp, TOKEN_LPAREN);

	char* loop_start = new_label(comp);
	char* loop_end = new_label(comp);

	emit_label(comp, loop_start);

	parse_expression(comp);

	expect_token(comp, TOKEN_RPAREN);

	char line[MAX_LINE_LEN];
	snprintf(line, sizeof(line), "JZ %s", loop_end);
	emit(comp, line);

	parse_statement(comp);

	snprintf(line, sizeof(line), "JMP %s", loop_start);
	emit(comp, line);

	emit_label(comp, loop_end);
}

void parse_expression_statement(compiler_t* comp) {
	if (current_token(comp)->type != TOKEN_SEMICOLON) {
		parse_expression(comp);
		
	}
	expect_token(comp, TOKEN_SEMICOLON);
}

void parse_expression(compiler_t* comp) {
	parse_comparison(comp);
}

void parse_comparison(compiler_t* comp) {
	parse_term(comp);

	while (true) {
		token_type_t op = current_token(comp)->type;
		if (op == TOKEN_EQUALS || op == TOKEN_NOT_EQUALS ||
			op == TOKEN_LESS || op == TOKEN_GREATER ||
			op == TOKEN_LESS_EQUAL || op == TOKEN_GREATER_EQUAL) {

			consume_token(comp);
			parse_term(comp);

			switch (op) {
				case TOKEN_EQUALS: emit(comp, "EQ"); break;
				case TOKEN_NOT_EQUALS: emit(comp, "NEQ"); break;
				case TOKEN_LESS: emit(comp, "LT"); break;
				case TOKEN_GREATER: emit(comp, "GT"); break;
				case TOKEN_LESS_EQUAL: emit(comp, "LTE"); break;
				case TOKEN_GREATER_EQUAL: emit(comp, "GTE"); break;
				default: break;
			}
		} else {
			break;
		}
	}
}

void parse_term(compiler_t* comp) {
	parse_multiplicative(comp);

	while (current_token(comp)->type == TOKEN_PLUS || 
		current_token(comp)->type == TOKEN_MINUS) {
		token_type_t op = current_token(comp)->type;
		consume_token(comp);
		parse_multiplicative(comp);

		if (op == TOKEN_PLUS) {
			emit(comp, "ADD");
		} else {
			emit(comp, "SUB");
		}
	}
}

void parse_factor(compiler_t* comp) {
	token_t* tok = current_token(comp);

	if (tok->type == TOKEN_EOF) {
		error(comp, "Unexpected end of file");
		return;
	}

	if (tok->type == TOKEN_NUMBER) {
		char line[MAX_LINE_LEN];
		snprintf(line, sizeof(line), "PUSH %s", tok->value);
		emit(comp, line);
		consume_token(comp);
	}
	else if (tok->type == TOKEN_IDENTIFIER) {
		char name[MAX_IDENTIFIER_LEN];
		strcpy(name, tok->value);
		consume_token(comp);

		if (current_token(comp)->type == TOKEN_LPAREN) {
			
			parse_function_call(comp, name);
		} else {
			
			symbol_t* sym = find_symbol(comp, name);
			if (!sym) {
				char error_msg[MAX_LINE_LEN];
				snprintf(error_msg, sizeof(error_msg), "Undefined variable '%s'", name);
				error(comp, error_msg);
			}

			char line[MAX_LINE_LEN];
			snprintf(line, sizeof(line), "LOAD 0x%04X", sym->address);
			emit(comp, line);
		}
	}
	else if (tok->type == TOKEN_LPAREN) {
		consume_token(comp);
		parse_expression(comp);
		expect_token(comp, TOKEN_RPAREN);
	}
	else {
		char error_msg[MAX_LINE_LEN];
		snprintf(error_msg, sizeof(error_msg), "Expected number, variable, or expression, got token type %d", tok->type);
		error(comp, error_msg);
	}
}

void parse_multiplicative(compiler_t* comp) {
	parse_factor(comp);

	while (current_token(comp)->type == TOKEN_MULTIPLY || 
		current_token(comp)->type == TOKEN_DIVIDE) {
		token_type_t op = current_token(comp)->type;
		consume_token(comp);
		parse_factor(comp);

		if (op == TOKEN_MULTIPLY) {
			emit(comp, "MUL");
		} else {
			emit(comp, "DIV");
		}
	}
}

void parse_function_call(compiler_t* comp, const char* func_name) {
	if (!is_builtin_function(func_name)) {
		char error_msg[MAX_LINE_LEN];
		snprintf(error_msg, sizeof(error_msg), "Unknown function '%s'", func_name);
		error(comp, error_msg);
	}

	expect_token(comp, TOKEN_LPAREN);

	int arg_count = 0;

	
	if (current_token(comp)->type != TOKEN_RPAREN && current_token(comp)->type != TOKEN_EOF) {
		parse_expression(comp);
		arg_count++;

		while (match_token(comp, TOKEN_COMMA)) {
			if (current_token(comp)->type == TOKEN_RPAREN || current_token(comp)->type == TOKEN_EOF) {
				error(comp, "Expected expression after comma");
			}
			parse_expression(comp);
			arg_count++;
		}
	}

	expect_token(comp, TOKEN_RPAREN);

	
	int syscall_id = get_builtin_syscall(func_name);
	if (syscall_id >= 0) {
		char line[MAX_LINE_LEN];

		if (strcmp(func_name, "halt") == 0) {
			emit(comp, "SYS 0x00");
			emit(comp, "HALT");
		} else {
			snprintf(line, sizeof(line), "SYS 0x%02X", syscall_id);
			emit(comp, line);
			
			
			if (strcmp(func_name, "read_char") == 0) {
				// read_char pushes the read character onto the stack
				// So we need to account for this in expression parsing
			}
			// Graphics calls like draw_pixel, draw_line, etc. consume stack but don't push
			// No additional stack manipulation needed
		}
	}
}

int compile_file(const char* input_file, const char* output_file) {
	if (!input_file || !output_file) {
		fprintf(stderr, "Error: NULL file path provided\n");
		return 1;
	}

	printf("Compiling: %s -> %s\n", input_file, output_file);

	FILE* input = fopen(input_file, "r");
	if (!input) {
		perror("fopen failed"); 
		fprintf(stderr, "Error: Cannot open input file '%s'\n", input_file);
		return 1;
	}
	
	printf("Input file opened successfully\n");

	fseek(input, 0, SEEK_END);
	long file_size = ftell(input);
	fseek(input, 0, SEEK_SET);

	if (file_size <= 0) {
		fprintf(stderr, "Error: Input file is empty or invalid\n");
		fclose(input);
		return 1;
	}

	char* source = malloc(file_size + 1);
	if (!source) {
		fprintf(stderr, "Error: Memory allocation failed\n");
		fclose(input);
		return 1;
	}

	size_t bytes_read = fread(source, 1, file_size, input);
	source[bytes_read] = '\0';
	fclose(input);

	
	
	printf("Allocating compiler structure...\n");
	compiler_t* comp = calloc(1, sizeof(compiler_t));
	if (!comp) {
		fprintf(stderr, "Error: Failed to allocate memory for compiler structure\n");
		free(source);
		return 1;
	}
	
	
	comp->next_var_addr = VAR_START_ADDR;
	comp->current_line = 1;

	
	printf("Tokenizing...\n");
	tokenize(comp, source);
	printf("Generated %d tokens\n", comp->token_count);

	
	printf("Parsing and generating code...\n");
	parse_program(comp);
	printf("Generated %d lines of assembly\n", comp->output_count);

	
	FILE* output = fopen(output_file, "w");
	if (!output) {
		fprintf(stderr, "Error: Cannot create output file '%s'\n", output_file);
		free(source);
		free(comp);
		return 1;
	}

	for (int i = 0; i < comp->output_count; i++) {
		fprintf(output, "%s\n", comp->output[i]);
	}

	fclose(output);
	free(source);
	free(comp);  

	printf("Compilation successful: %s -> %s\n", input_file, output_file);
	return 0;
}


bool is_valid_string(const char* str, size_t max_len) {
	if (!str) return false;
	
	
	for (size_t i = 0; i < max_len; i++) {
		if (str[i] == '\0') {
			return i > 0; 
		}
		
		if (str[i] < 0 || (unsigned char)str[i] > 127) {
			return false;
		}
	}
	return false; 
}

int main(int argc, char* argv[]) {
	if (argc != 3) {
		printf("TinyC Compiler v1.0\n");
		printf("Usage: %s <input.tc> <output.asm>\n", argv[0] ? argv[0] : "compiler");
		return 1;
	}

	
	if (!argv[1] || !argv[2]) {
		fprintf(stderr, "Error: Invalid command line arguments (NULL pointers)\n");
		return 1;
	}

	
	if (!is_valid_string(argv[1], 1024)) {
		fprintf(stderr, "Error: Input filename is invalid or not properly null-terminated\n");
		return 1;
	}

	if (!is_valid_string(argv[2], 1024)) {
		fprintf(stderr, "Error: Output filename is invalid or not properly null-terminated\n");
		return 1;
	}

	
	size_t input_len = strlen(argv[1]);
	size_t output_len = strlen(argv[2]);
	
	if (input_len > 512 || output_len > 512) {
		fprintf(stderr, "Error: File paths too long (max 512 characters)\n");
		return 1;
	}

	
	char input_file[513];
	char output_file[513];
	
	strncpy(input_file, argv[1], 512);
	input_file[512] = '\0'; 
	
	strncpy(output_file, argv[2], 512);
	output_file[512] = '\0'; 

	printf("Input file: '%s' (length: %zu)\n", input_file, strlen(input_file));
	printf("Output file: '%s' (length: %zu)\n", output_file, strlen(output_file));

	return compile_file(input_file, output_file);
}
