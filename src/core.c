#include "cical.h"

#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "compute.h"


#define countof(arr) (int)(sizeof(arr) / sizeof(arr[0]))

static const alpha_name_t ALPHA_NAMES[] = {
	{ "alpha" }, { "beta" }, { "gamma" }, { "zeta" }, { "mu" }, 
	{ "pi" }, { "rho" }, { "tau" }, { "phi" }, { "psi" }
};
const alpha_name_t *get_alpha_name(const char *str)
{
	for (int i = 0; i < countof(ALPHA_NAMES); i++)
	{
		if (!strncmp(str, ALPHA_NAMES[i].str, strlen(ALPHA_NAMES[i].str)))
			return &ALPHA_NAMES[i];
	}
	return NULL;
}


/* --------------------- S- & L- FUNCTIONS --------------------- */

#define SFUNCS_LIST \
	X("sqrt",		sqrt  ) \
	X("cbrt",		cbrt  ) \
	X("ln",			log   ) \
	X("lg",			log10 ) \
	X("exp",		exp   ) \
	X("erf",		erf   ) \
	X("arcsinh",	asinh ) \
	X("arccosh",	acosh ) \
	X("arctanh",	atanh ) \
	X("arccoth",	acoth ) \
	X("arcsin",		asin  ) \
	X("arccos",		acos  ) \
	X("arctan",		atan  ) \
	X("arccot",		acot  ) \
	X("sinh",		sinh  ) \
	X("cosh",		cosh  ) \
	X("tanh",		tanh  ) \
	X("coth",		coth  ) \
	X("sin",		sin   ) \
	X("cos",		cos   ) \
	X("tan",		tan   ) \
	X("cot",		cot   ) \
	X("floor",		floor ) \
	X("ceil",		ceil  ) \
	X("sign",		sign  )

static sfunc_t SFUNCS[] = {
#define X(name, func) { name, .logic = NULL },
	SFUNCS_LIST
#undef X
};
const sfunc_t *get_sfunc(const char *str)
{
	for (int i = 0; i < countof(SFUNCS); i++)
	{
		if (!strncmp(str, SFUNCS[i].name, strlen(SFUNCS[i].name)))
			return &SFUNCS[i];
	}
	return NULL;
}
static void declare_sfuncs(void)
{
	int i = 0;   /* silly msvc can not locate functions at compile time */
#define X(name, func) SFUNCS[i++].logic = func;
	SFUNCS_LIST
#undef X
}

static const lfunc_t LFUNCS[] = {
	{ "log", llog },
	{ "max", lmax }, { "min", lmin },
	/* int funcs */
	{ "lcm", llcm }, { "gcd", lgcd },
	{ "mod", lmod }, { "pow", lpow }, { "inv", linv },
};
const lfunc_t *get_lfunc(const char *str)
{
	for (int i = 0; i < countof(LFUNCS); i++)
	{
		if (!strncmp(str, LFUNCS[i].name, strlen(LFUNCS[i].name)))
			return &LFUNCS[i];
	}
	return NULL;
}


/* --------------------- VARIABLES --------------------- */

static struct
{
	identifier_t ident;
	ast_node_t *value;
	int mut;
} VARSET[1024] = { 0 };

static void insert_new_variable(identifier_t ident, ast_node_t *value, int mut)
{
	int i = 0;
	for (; i < countof(VARSET); i++)
	{
		if ((ident_eq(VARSET[i].ident, ident) || VARSET[i].ident.type == 0) && VARSET[i].mut) break;
	}

	if (i == countof(VARSET)) PANIC("Varset ended");

	annihilate_tree(VARSET[i].value);
	VARSET[i].ident = ident;
	VARSET[i].value = value;
	VARSET[i].mut = mut;
}
const struct ast_node *get_variable(identifier_t ident)
{
	for (int i = 0; i < countof(VARSET); i++)
	{
		if (ident_eq(VARSET[i].ident, ident)) return VARSET[i].value;
	}
	return NULL;
}
int remove_variables(void)
{
	int count = 0;
	for (int i = 0; i < countof(VARSET); i++)
	{
		if (VARSET[i].mut)
		{
			if (VARSET[i].value) count++;
			annihilate_tree(VARSET[i].value);
			VARSET[i].value = NULL;
			VARSET[i].ident = (identifier_t){ 0 };
		}
	}
	return count;
}


/* --------------------- FUNCTIONS --------------------- */

static dfunc_t FUNCSET[1024] = { 0 };
const dfunc_t *get_dfunc(identifier_t ident)
{
	for (int i = 0; i < countof(FUNCSET); i++)
	{
		if (ident_eq(FUNCSET[i].ident, ident)) return &FUNCSET[i];
	}
	return NULL;
}
static int insert_new_dfunc(identifier_t ident, ast_node_t *args, ast_node_t *impl)
{
	int i = 0;
	for (; i < countof(FUNCSET); i++)
	{
		if (ident_eq(FUNCSET[i].ident, ident) || FUNCSET[i].ident.type == 0) break;
	}

	if (i == countof(FUNCSET)) PANIC("Funcset ended");

	const ast_node_t *temp = args;
	while (temp)
	{
		if (temp->left->type != NODE_VARIABLE) return 1;
		temp = temp->right;
	}

	FUNCSET[i].ident = ident;
	annihilate_tree(FUNCSET[i].args);
	annihilate_tree(FUNCSET[i].impl);
	FUNCSET[i].args = args;
	FUNCSET[i].impl = impl;
	return 0;
}
int remove_dfuncs(void)
{
	int count = 0;
	for (int i = 0; i < countof(FUNCSET); i++)
	{
		if (FUNCSET[i].impl) count++;
		FUNCSET[i].ident = (identifier_t) { 0 };
		annihilate_tree(FUNCSET[i].args);
		annihilate_tree(FUNCSET[i].impl);
		FUNCSET[i].args = NULL;
		FUNCSET[i].impl = NULL;
	}
	return count;
}


/* --------------------- MISC --------------------- */

int ident_eq(identifier_t a, identifier_t b)
{
	return a.type == b.type && a.alpha == b.alpha && a.symbol == b.symbol && a.subscript == b.subscript;
}
void annihilate_tree(struct ast_node *node)
{
	if (!node) return;
	if (node->left) annihilate_tree(node->left);
	if (node->right) annihilate_tree(node->right);
	
	if (node->type == NODE_NUMBER)
		number_free(node->number);

	free(node);
}
static void print_identifier(identifier_t i)
{
	if (i.type == IDENTIFIER_SYMBOL)
		printf("%c", i.symbol);
	else if (i.type == IDENTIFIER_ALPHA)
		printf("%s", i.alpha->str);

	if (i.subscript != -1)
		printf("%d", i.subscript);
}
void show_core(char c)
{
	if (c == 0 || c == 'v')
	{
		printf("Variables: ");
		int total = 0;
		for (int i = 0; i < countof(VARSET); i++)
		{
			if (VARSET[i].mut && VARSET[i].ident.type)
			{
				print_identifier(VARSET[i].ident);
				printf(", ");
				total++;
			}
		}
		if (total == 0) printf("none");
		putchar('\n');
	}
	if (c == 0 || c == 'f')
	{
		printf("Functions: ");
		int total = 0;
		for (int i = 0; i < countof(FUNCSET); i++)
		{
			if (!FUNCSET[i].ident.type) continue;

			print_identifier(FUNCSET[i].ident);
			putchar('(');
			const ast_node_t *arg = FUNCSET[i].args;
			while (arg->right)
			{
				print_identifier(arg->left->ident);
				printf(", ");
				arg = arg->right;
			}
			print_identifier(arg->left->ident);
			printf("); ");
			total++;
		}
		if (total == 0) printf("none");
		putchar('\n');
	}
}
void number_free(number_t num)
{
	if (num.type == NUMBER_BIGINT)
		bi_free(&num.bint);
}
number_t number_copy(number_t num)
{
	if (num.type == NUMBER_DOUBLE)
		return num;
	else if (num.type == NUMBER_BIGINT)
	{
		/* do NOT transfer ownership */
		bigint_t cpy = { 0 };
		bi_copy(&cpy, &num.bint);
		return (number_t) { .type = NUMBER_BIGINT, .bint = cpy };
	}
	else return (number_t){ 0 };
}

static compresult_t PREVIOUS_ANSWER = { .value = { { 0 }, 0 }, .error = ERROR_COMPUTE_NO_PREVIOUS_ANSWER_KNOWN };
struct compresult get_previous_answer(void)
{
	return (compresult_t) { .error = PREVIOUS_ANSWER.error, .value = number_copy(PREVIOUS_ANSWER.value) };
}


/* --------------------- PRELOAD --------------------- */

static void preload_consts(identifier_t ident, double value)
{
	ast_node_t *node = calloc(1, sizeof(ast_node_t));
	if (!node) PANIC("Undefined unallocation");
	node->type = NODE_NUMBER;
	node->number.type = NUMBER_DOUBLE;
	node->number.doble = value;
	insert_new_variable(ident, node, 0);
}
void preload_defaults(void)
{
	for (int i = 0; i < countof(VARSET); i++) VARSET[i].mut = 1;

	preload_consts((identifier_t) { .type = IDENTIFIER_SYMBOL, .symbol = 'e', .subscript = -1 },
		2.71828182845904523536028747135);
	preload_consts((identifier_t) { .type = IDENTIFIER_ALPHA, .alpha = get_alpha_name("pi"), .subscript = -1 },
		3.14159265358979323846264338328);
	preload_consts((identifier_t) { .type = IDENTIFIER_ALPHA, .alpha = get_alpha_name("tau"), .subscript = -1 },
		6.283185307179586476925286766559);
	preload_consts((identifier_t) { .type = IDENTIFIER_ALPHA, .alpha = get_alpha_name("phi"), .subscript = -1 },
		1.61803398874989484820458683436);
	preload_consts((identifier_t) { .type = IDENTIFIER_ALPHA, .alpha = get_alpha_name("gamma"), .subscript = -1 },
		-0.57721566490153286060651209008);

	declare_sfuncs();
}


/* --------------------- MAIN --------------------- */

int execute(const struct ast_node *root, struct compresult *cr)
{
	if (root->type == NODE_ASSIGNMENT)
	{
		if (root->left->type == NODE_VARIABLE)
		{
			insert_new_variable(root->left->ident, root->right, 1);
			return EXECUTE_FREE_DEFINABLE;
		}
		else if (root->left->type == NODE_DFUNCTION)
		{
			if (insert_new_dfunc(root->left->ident, root->left->left, root->right))
			{
				cr->error = ERROR_COMPUTE_EXPECTED_ONLY_VARIABLES;
				return EXECUTE_ANNIHILATE_TREE | EXECUTE_PRINT_RESULT;
			}
			else return EXECUTE_FREE_DEFINABLE;
		}
		else
		{
			cr->error = -1;
			return EXECUTE_ANNIHILATE_TREE;
		}
	}
	else
	{
		*cr = compute_node(root, NULL);
		if (!cr->error)
		{
			number_free(PREVIOUS_ANSWER.value);
			PREVIOUS_ANSWER = *cr;
		}
		return EXECUTE_ANNIHILATE_TREE | EXECUTE_PRINT_RESULT;
	}
}
