#pragma once

#include "cical.h"

enum
{
	TOKEN_DIGITS = 1,
	TOKEN_SYMBOL,
	TOKEN_ALPHA,
	TOKEN_SFUNC,
	TOKEN_LFUNC,
	TOKEN_OPERATION,
	TOKEN_BRACKET,
	TOKEN_UNDERSCORE,
	TOKEN_COMMA,
	TOKEN_DOT,
	TOKEN_BAR,
	TOKEN_AT,
	TOKEN_EQUAL,
	TOKEN_EOL
};

typedef struct token
{
	int type;
	union
	{
		char sym;
		char *str;
		void *ptr;
	};
} token_t;

int tokenize_line(const char *line, token_t *tokens, int maxsize);
void cleanup_tokens(token_t *tokens, int count);
