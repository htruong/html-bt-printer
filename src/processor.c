#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum Token_Type {
	TOKEN_HTML_BEGIN = 15, TOKEN_HTML_END = 16,
	TOKEN_HEADER_BEGIN = 20, TOKEN_HEADER_END = 21,
	TOKEN_META = 80,
	TOKEN_BODY_BEGIN = 30, TOKEN_BODY_END = 31, 
	TOKEN_SMALL_BEGIN = 60, TOKEN_SMALL_END = 61, 
	TOKEN_BIG_BEGIN = 35, TOKEN_BIG_END = 36,
	TOKEN_B_BEGIN = 25, TOKEN_B_END = 26,
	TOKEN_I_BEGIN = 40, TOKEN_I_END = 41,
	TOKEN_U_BEGIN = 55, TOKEN_U_END = 56,
	TOKEN_INVERT_BEGIN = 45, TOKEN_INVERT_END = 46,
	TOKEN_IMG = 50,
	TOKEN_INVALID = 254
};

enum Parser_State {
	PARSER_TOKENIZER,
	PARSER_PASSTHRU_TEXT,
	PARSER_PASSTHRU_TEXT_FORCE,
	PARSER_PASSTHRU_IMG_BASE64
};


enum Tokenizer_State {
	TK_START = 0,
	TK_H = 10, TK_HTML = 15, TK_HEAD = 20,
	TK_B = 25, TK_BODY = 30, TK_BIG = 35,
	TK_I = 40, TK_INVERT = 45, TK_IMG = 50,
	TK_U = 55,
	TK_SMALL = 60,
	TK_LEFT = 65,
	TK_RIGHT = 70,
	TK_CENTER = 75,
	TK_META = 80,
	TK_INVALID = 254
};


char buf[32];
unsigned char buf_count;
enum Tokenizer_State ts = TK_START;
enum Parser_State ps = PARSER_TOKENIZER;
unsigned char token_closetag = 0;

struct token {
	enum Token_Type type;
	char * key;
	char * value;
};

void _printc(char c) 
{
	putchar(c);
}

void _prints(char* s) 
{
	printf("%s", s);
}

void printToken(struct token t)
{
	if (t.type == TOKEN_INVALID) {
		printf("[INVALID]");
	} else {
		printf("[%d]", t.type);
	}
}

int consume(char c) 
{
	if (ps == PARSER_PASSTHRU_TEXT) {
		if (c == '\\') {
			ps = PARSER_PASSTHRU_TEXT_FORCE;
		} else  if (c == '<') {
			ps = PARSER_TOKENIZER;
			ts = TK_START;
			token_closetag = 0;
		} else {
			_printc(c);
		}
	} else if (ps == PARSER_PASSTHRU_TEXT_FORCE) {
		_printc(c);
		ps = PARSER_PASSTHRU_TEXT;
	} else if (ps == PARSER_PASSTHRU_IMG_BASE64) {

	} else {
		if (c == '<') {
			ts = TK_START;
		} else if (c == '>') {
			struct token t;
			t.type = ts;
			if (token_closetag) {
				t.type += 1;
			}
			printToken(t);
			// reset state
			ps = PARSER_PASSTHRU_TEXT;
			ts = TK_START;
			token_closetag = 0;
		} else if (c == '/') {
			if (ts == TK_START)  {
				token_closetag = 1;
			}
		} else if (c == 'h') {
			if (ts == TK_START)  {
				ts = TK_H;
			}
		} else if (c == 't') {
			if (ts == TK_H)  {
				ts = TK_HTML;
			}
		} else if (c == 'e') {
			if (ts == TK_H)  {
				ts = TK_HEAD;
			}
		} else if (c == 'b') {
			if (ts == TK_START)  {
				ts = TK_B;
			}
		} else if (c == 'i') {
			if (ts == TK_B)  {
				ts = TK_BIG;
			} else if (ts == TK_START) {
				ts = TK_I;
			}
		} else if (c == 'o') {
			if (ts == TK_B)  {
				ts = TK_BODY;
			}
		} else if (c == 'm') {
			if (ts == TK_START)  {
				ts = TK_META;
			} else if (ts == TK_I) {
				ts = TK_IMG;
			}
		} else if (c == 'n') {
			if (ts == TK_I) {
				ts = TK_INVERT;
			}
		} else if (c == 's') {
			if (ts == TK_START) {
				ts = TK_SMALL;
			}
		} else if (c == 'l') {
			if (ts == TK_START) {
				ts = TK_LEFT;
			}
		} else if (c == 'r') {
			if (ts == TK_START) {
				ts = TK_RIGHT;
			}
		} else if (c == 'c') {
			if (ts == TK_START) {
				ts = TK_CENTER;
			}
		} else {
			if (ts == TK_START) {
				ts = TK_INVALID;
			}
		}
	}
}



#define BUF_SIZE 1024


int main() 
{
	char buffer[BUF_SIZE];
	size_t contentSize = 1; // includes NULL
	size_t i = 0;
	/* Preallocate space.  We could just allocate one char here, 
	but that wouldn't be efficient. */
	char *content = malloc(sizeof(char) * BUF_SIZE);
	if(content == NULL)
	{
	    perror("Failed to allocate content");
	    exit(1);
	}
	content[0] = '\0'; // make null-terminated
	while(fgets(buffer, BUF_SIZE, stdin))
	{
	    char *old = content;
	    contentSize += strlen(buffer);
	    content = realloc(content, contentSize);
	    if(content == NULL)
	    {
	        perror("Failed to reallocate content");
	        free(old);
	        exit(2);
	    }
	    strcat(content, buffer);
	}

	for (i = 0; i < contentSize; i ++) {
		consume(content[i]);
	}

	printf("\nDone!\n");
}
