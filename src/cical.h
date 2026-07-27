#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bignum.h"

/* --------------------- ERRORS --------------------- */

#define PANIC(s) { fprintf(stderr, s); exit(__COUNTER__); }

#define ERROR_LEXER_UNDEFINED_SYMBOL 0xc1ca1100

#define ERROR_PARSER_EXPECTED_NUMBER_AFTER_DOT 0xc1ca1200
#define ERROR_PARSER_EXPECTED_NUMBER_TYPE 0xc1ca1201
#define ERROR_PARSER_UNCLOSED_BRACKET 0xc1ca1202
#define ERROR_PARSER_EXPECTED_ARGUMENT 0xc1ca1203
#define ERROR_PARSER_EXPECTED_DEFINITION 0xc1ca1204
#define ERROR_PARSER_NOT_FINISHED_STATEMENT 0xc1ca1205

#define ERROR_COMPUTE_UNDEFINED_VARIABLE 0xc1ca1300
#define ERROR_COMPUTE_TOO_FEW_ARGUMENTS 0xc1ca1301
#define ERROR_COMPUTE_EXPECTED_ONLY_VARIABLES 0xc1ca1302
#define ERROR_COMPUTE_NO_PREVIOUS_ANSWER_KNOWN 0xc1ca1303
#define ERROR_COMPUTE_ILLEGAL_NUMBER_TYPE 0xc1ca1304
#define ERROR_COMPUTE_CONVERSION_OF_NUMBER_TYPES 0xc1ca1305
#define ERROR_COMPUTE_DIVISION_BY_ZERO 0xc1ca1306
#define ERROR_COMPUTE_NO_MINV_EXIST 0xc1ca1307

/* --------------------- CORE --------------------- */

/* long names */
typedef struct alpha_name
{
	const char *str;
} alpha_name_t;
const alpha_name_t *get_alpha_name(const char *str);

/* identifiers */
typedef struct identifier
{
	union
	{
		char symbol;
		const alpha_name_t *alpha;
	};
	enum
	{
		IDENTIFIER_SYMBOL = 1,
		IDENTIFIER_ALPHA
	} type;
	int subscript;
} identifier_t;
int ident_eq(identifier_t a, identifier_t b);

/* numbers */
typedef struct number
{
	union
	{
		double doble;
		bigint_t bint;
	};
	enum
	{
		NUMBER_DOUBLE = 1,
		NUMBER_BIGINT,
	} type;
} number_t;
void number_free(number_t num);
number_t number_copy(number_t num);

/* variables */
const struct ast_node *get_variable(identifier_t ident);
int remove_variables(void);

/* functions */
struct ast_node;
struct compresult;
struct varenv;

typedef struct sfunc
{
	const char *name;
	double(*logic)(double);
} sfunc_t;
const sfunc_t *get_sfunc(const char *str);

typedef struct lfunc
{
	const char *name;
	struct compresult(*logic)(const struct ast_node *node, struct varenv *env);
} lfunc_t;
const lfunc_t *get_lfunc(const char *str);

typedef struct dfunc
{
	identifier_t ident;
	struct ast_node *args;
	struct ast_node *impl;
} dfunc_t;
const dfunc_t *get_dfunc(identifier_t ident);
int remove_dfuncs(void);

/* main */
#define EXECUTE_ANNIHILATE_TREE 0x1
#define EXECUTE_PRINT_RESULT 0x2
#define EXECUTE_FREE_DEFINABLE 0x4
int execute(const struct ast_node *root, struct compresult *cr);

void preload_defaults(void);
void annihilate_tree(struct ast_node *node);
void show_core(char c);
struct compresult get_previous_answer(void);
