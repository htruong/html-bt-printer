#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 1024

#define INVERSE_MASK       (1 << 1)
#define UPDOWN_MASK        (1 << 2)
#define BOLD_MASK          (1 << 3)
#define DOUBLE_HEIGHT_MASK (1 << 4)
#define DOUBLE_WIDTH_MASK  (1 << 5)
#define STRIKE_MASK        (1 << 6)

// Barcode types
#define UPC_A   0
#define UPC_E   1
#define EAN13   2
#define EAN8    3
#define CODE39  4
#define I25     5
#define CODEBAR 6
#define CODE93  7
#define CODE128 8
#define CODE11  9
#define MSI    10

enum Token_Type {
	TOKEN_HTML_BEGIN = 15, TOKEN_HTML_END = 16,
	TOKEN_HEADER_BEGIN = 20, TOKEN_HEADER_END = 21,
	TOKEN_META = 80,
	TOKEN_BODY_BEGIN = 30, TOKEN_BODY_END = 31, 
	TOKEN_BIG_BEGIN = 35, TOKEN_BIG_END = 36,
	TOKEN_B_BEGIN = 25, TOKEN_B_END = 26,
	TOKEN_BR = 27,
	TOKEN_BARCODE = 28, 
	TOKEN_I_BEGIN = 40, TOKEN_I_END = 41,
	TOKEN_U_BEGIN = 55, TOKEN_U_END = 56,
	TOKEN_S_BEGIN = 60, TOKEN_S_END = 61,
	TOKEN_SMALL_BEGIN = 63, TOKEN_SMALL_END = 64,
	TOKEN_INVERT_BEGIN = 45, TOKEN_INVERT_END = 46,
	TOKEN_LEFT_BEGIN = 65, TOKEN_LEFT_END = 66,
	TOKEN_RIGHT_BEGIN = 70, TOKEN_RIGHT_END = 71,
	TOKEN_CENTER_BEGIN = 75, TOKEN_CENTER_END = 76,
	TOKEN_IMG = 50,
	TOKEN_INVALID = 254
};

enum Parser_State {
	PARSER_TOKENIZER,
	PARSER_PASSTHRU_TEXT,
	PARSER_PASSTHRU_TEXT_FORCE,
	PARSER_PASSTHRU_BARCODE,
	PARSER_PASSTHRU_IMG_BASE64
};


enum Tokenizer_State {
	TK_START = 0,
	TK_H = 210, TK_HTML = 15, TK_HEAD = 20,
	TK_B = 25, TK_BODY = 30, TK_BIG = 35, TK_BR = 27, TK_BARCODE = 228,
	TK_I = 40, TK_INVERT = 45, TK_IMG = 50,
	TK_U = 55,
	TK_S = 60, TK_SMALL = 63, 
	TK_LEFT = 65,
	TK_RIGHT = 70,
	TK_CENTER = 75,
	TK_META = 80,
	TK_INVALID = 200
};

char buf[32];
unsigned char buf_count;
enum Tokenizer_State ts = TK_START;
enum Parser_State ps = PARSER_TOKENIZER;
unsigned char token_closetag = 0;
int print_mode = 0;
int i = 0;
int _dotPrintTime = 0;
int _dotFeedTime = 0;

struct token {
	enum Token_Type type;
	char * key;
	char * value;
};

void _printc(char c) 
{
	putchar(c);
	//printf("/%d ", c);
}

void _prints(char* s) 
{
	printf("%s", s);
}

void _write_print_mode() {
	_printc(27);
	_printc(33);
	_printc(print_mode);
}

void _set_print_mode(unsigned char mask) {
	print_mode |= mask;
	_write_print_mode();
//  charHeight = (print_mode & DOUBLE_HEIGHT_MASK) ? 48 : 24;
//  maxColumn  = (print_mode & DOUBLE_WIDTH_MASK ) ? 16 : 32;
}

void _unset_print_mode(unsigned char mask) {
	print_mode &= ~mask;
	_write_print_mode();
//  charHeight = (print_mode & DOUBLE_HEIGHT_MASK) ? 48 : 24;
//  maxColumn  = (print_mode & DOUBLE_WIDTH_MASK ) ? 16 : 32;
}

void _set_align(enum Token_Type t) {
	unsigned int mode_char = 0;
	switch(t) {
		case TOKEN_CENTER_BEGIN: mode_char = 1; break;
		case TOKEN_RIGHT_BEGIN: mode_char = 2; break;
		default: mode_char = 0; break;
	}
	_printc(0x1B);
	_printc(0x61);
	_printc(mode_char);

}

void _reset_printer() {
	_printc(0xFF);
	for(i=0; i<10; i++) {
		usleep(1000);
		_printc(27);
	}
	_printc(27);
	_printc(64);
	print_mode = 0x0;
	_printc(27);
	_printc(55);
	_printc(20);
	_printc(255);
	_printc(250);
	_printc(18);
	_printc(35);
	_printc(4 << 5 || 14);
	_dotPrintTime = 30000; // See comments near top of file for
	_dotFeedTime  =  2100; // an explanation of these values.
}



unsigned char in[4];
unsigned char out[3];
unsigned char incount = 0;
unsigned char pad_count = 0;

unsigned char imgdata_pos = 0;



void decodeblock( unsigned char *in, unsigned char *out )
{
    out[ 0 ] = (unsigned char ) (in[0] << 2 | in[1] >> 4);
    out[ 1 ] = (unsigned char ) (in[1] << 4 | in[2] >> 2);
    out[ 2 ] = (unsigned char ) (((in[2] << 6) & 0xc0) | in[3]);
}

unsigned int img_w, img_h;
int rowBytes, rowBytesClipped;
unsigned int img_start = 0;
unsigned int chunk_data_left = 0;
unsigned int misaligned_offset = 0;

void consume_imagedata (unsigned char c) {
	//fprintf(stderr, "%c", c);
	if (imgdata_pos < 4) {
		//fprintf(stderr, "Got bit %d (%u)\n", imgdata_pos, (unsigned int)c);
		switch (imgdata_pos) {

			case 0:
				img_w = c;
				break;
			case 1:
				img_w |= (((unsigned int)c) << 8);
			case 2:
				img_h = c;
				break;
			case 3:
				img_h |= (((unsigned int)c) << 8);
				fprintf(stderr, "The image width and height is (%u x %u)\n", img_w, img_h);
				rowBytes        = (img_w + 7) / 8; // Round up to next byte boundary
  				rowBytesClipped = (rowBytes >= 48) ? 48 : rowBytes; // 384 pixels max width
  				img_start = 0;
  				chunk_data_left = 0;
				break;
		}
		imgdata_pos ++;
	} else {
		if (chunk_data_left <= 0) {
  			sleep(1);
			unsigned int chunkHeight = img_h - img_start;
			if (chunkHeight <= 0) {
				fprintf(stderr, "Still receiving ([%c]) -- Discarding! \n", c);
				return;
			}
  			if(chunkHeight > 255) chunkHeight = 255;
  			fprintf(stderr, "Printing chunk of data of size (%u x %u)\n", rowBytesClipped * 8, chunkHeight);
  			_printc(18); _printc(42); _printc(chunkHeight); _printc(rowBytesClipped);
  			chunk_data_left = chunkHeight * rowBytesClipped;
  			img_start += chunkHeight;
  		} 
  		_printc(c);
  		usleep(100);
  		chunk_data_left -= 1;
	}
}

char decodeCharacterTable[256] = {
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
	,-1,62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21
	,22,23,24,25,-1,-1,-1,-1,-1,-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
	,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1}; 

void consume_decode( char v )
{
	if (v == 0) {
		// End of stream, we should decode any remaining char
		if (pad_count > 0) {
			decodeblock(in, out);
			if (pad_count == 1) {
				consume_imagedata(out[0]);
				consume_imagedata(out[1]);
			} else { // pad_count == 2
				consume_imagedata(out[0]);
			}
		}
	} else if (v == '=') {
		pad_count++;
	} else {
		if (decodeCharacterTable[v] != -1) {
			in[incount++] = decodeCharacterTable[v];
			if (incount == 4) {
				decodeblock(in, out);
				consume_imagedata(out[0]);
				consume_imagedata(out[1]);
				consume_imagedata(out[2]);
				incount = 0;
			}
		}
	}
}

void _processToken(struct token t)
{
	switch (t.type) {
		case TOKEN_HTML_BEGIN:
		_reset_printer();
		break;

		case TOKEN_INVERT_BEGIN:
		_set_print_mode(INVERSE_MASK);
		break;

		case TOKEN_INVERT_END:
		_unset_print_mode(INVERSE_MASK);
		break;

		case TOKEN_B_BEGIN:
		_set_print_mode(BOLD_MASK);
		break;

		case TOKEN_B_END:
		_unset_print_mode(BOLD_MASK);
		break;

		case TOKEN_S_BEGIN:
		_set_print_mode(STRIKE_MASK);
		break;

		case TOKEN_S_END:
		_unset_print_mode(STRIKE_MASK);
		break;

		case TOKEN_BR:
		_printc('\n');
		break;

		case TOKEN_U_BEGIN:
		_printc(27);
		_printc(45);
		_printc(1);
		break;

		case TOKEN_U_END:
		_printc(27);
		_printc(45);
		_printc(0);
		break;

		case TOKEN_CENTER_BEGIN:
		case TOKEN_CENTER_END:
		case TOKEN_LEFT_BEGIN:
		case TOKEN_LEFT_END:
		case TOKEN_RIGHT_BEGIN:
		case TOKEN_RIGHT_END:
		_set_align(t.type);
		break;

		default:
		//printf("[INVALID TOKEN %d]", t.type);
		break;
	}
}

int consume(char c) 
{
	if (ps == PARSER_PASSTHRU_TEXT) {
		// The passthru mode will send directly what it receives to the printer
		if (c == '\\') {
			// Sees \, eats the \ and passes thru the next character
			ps = PARSER_PASSTHRU_TEXT_FORCE;
		} else  if (c == '<') {
			// Sees <, eats the < and switch to the tokenizer mode
			ps = PARSER_TOKENIZER;
			ts = TK_START;
			token_closetag = 0;
		} else if (c == '\n') {
			// Eats the \n
		} else { // Passes thru
			_printc(c);
		}
	} else if (ps == PARSER_PASSTHRU_TEXT_FORCE) {
		// Force mode
		_printc(c);
		ps = PARSER_PASSTHRU_TEXT;
	} else if (ps == PARSER_PASSTHRU_BARCODE) {
		// Barcode
		if (c == '"') {
			_printc(0);
			// reset the parser state
			ps = PARSER_TOKENIZER;
			ts = TOKEN_INVALID;
			token_closetag = 0;
		} else {
			_printc(c);
		}
	} else if (ps == PARSER_PASSTHRU_IMG_BASE64) {
		// Img Base64
		if (c == '"') {
			consume_decode(0);
			// reset the parser state
			ps = PARSER_TOKENIZER;
			ts = TOKEN_INVALID;
			token_closetag = 0;
		} else {
			consume_decode(c);
		}
	} else {
		// Real tokenizer mode: tokenizes what it receives
		if (c == '<') {
			ts = TK_START;
		} else if (c == '>') {
			// Sees a closing >
			struct token t;
			t.type = ts;
			if (token_closetag) {
				t.type += 1;
			}
			_processToken(t);
			// reset state
			ps = PARSER_PASSTHRU_TEXT;
			ts = TK_START;
			token_closetag = 0;
		} else if (c == '/') {
			if (ts == TK_START)  { // We're closing the tag
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
		} else if (c == 'a') {
			if (ts == TK_B) {
				ts = TK_BARCODE;
			}
		} else if (c == '"') {
			if (ts == TK_BARCODE) {
				fprintf(stderr, "Printing barcode!\n");
				_printc(29), _printc(72), _printc(2);    // Print label below barcode
	  			_printc(29), _printc(119), _printc(3);    // Barcode width
	  			_printc(29), _printc(107), _printc(CODE128); // Barcode type (listed in .h file)
	  			ps = PARSER_PASSTHRU_BARCODE;
			} else if (ts == TK_IMG) {
				fprintf(stderr, "Processing image data!\n");
				ps = PARSER_PASSTHRU_IMG_BASE64;

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
			} else if (ts == TK_S) {
				ts = TK_SMALL;
			}
		} else if (c == 'n') {
			if (ts == TK_I) {
				ts = TK_INVERT;
			}
		} else if (c == 's') {
			if (ts == TK_START) {
				ts = TK_S;
			}
		} else if (c == 'l') {
			if (ts == TK_START) {
				ts = TK_LEFT;
			}
		} else if (c == 'r') {
			if (ts == TK_START) {
				ts = TK_RIGHT;
			} else if (ts == TK_B) {
				ts = TK_BR;
			}
		} else if (c == 'c') {
			if (ts == TK_START) {
				ts = TK_CENTER;
			}
		} else if (c == 'u') {
			if (ts == TK_START) {
				ts = TK_U;
			}
		} else {
			if (ts == TK_START) {
				ts = TK_INVALID;
			}
		}
	}
}



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

	//printf("\nDone!\n");
}
