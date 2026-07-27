#pragma once

#include "cical.h"

enum
{
	NODE_NUMBER = 1,
	NODE_VARIABLE,
	NODE_NEGATE,
	NODE_POW,
	NODE_MULTIPLY,
	NODE_DIVIDE,
	NODE_ADD,
	NODE_SUBTRACT,
	NODE_ABS,
	NODE_SFUNCTION,
	NODE_LFUNCTION,
	NODE_DFUNCTION,
	NODE_ARGUMENT_JOINT,
	NODE_PREVANSWER,
	NODE_ASSIGNMENT
};

typedef struct ast_node
{
	int type;
	union
	{
		number_t number;
		identifier_t ident;
		const sfunc_t *sfunc;
		const lfunc_t *lfunc;
	};
	struct ast_node *left, *right;
} ast_node_t;


struct token;
int parse_content(const struct token *tokens, ast_node_t *root);
