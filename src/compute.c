#include "compute.h"
#include "parser.h"
#include "lexer.h"


static void collapse_env(varenv_t *env)
{
	if (!env) return;
	if (env->next) collapse_env(env->next);
	number_free(env->result.value);
	free(env);
}

#define COMRES_V(v) (compresult_t){ v, .error = 0 }
#define COMRES_VD(v) (compresult_t){ .value = { .type = NUMBER_DOUBLE, .doble = v }, 0 }
#define COMRES_VBI(v) (compresult_t){ .value = { .type = NUMBER_BIGINT, .bint = v }, 0 }
#define COMRES_E(e) (compresult_t){ .value = { { 0 }, 0 }, e }

static compresult_t convert_to_double(compresult_t cr)
{
	if (cr.value.type == NUMBER_DOUBLE) return cr;
	else if (cr.value.type == NUMBER_BIGINT)
	{
		cr.value.type = NUMBER_DOUBLE;
		/* temp save to prevent freeing cr.value.doble */
		double x = bi_as_double(&cr.value.bint);
		bi_free(&cr.value.bint);
		cr.value.doble = x;
		return cr;
	}
	else return COMRES_E(ERROR_COMPUTE_CONVERSION_OF_NUMBER_TYPES);
}


/* helpers */
static compresult_t compute_variable(const ast_node_t *node, varenv_t *env);
static compresult_t compute_negate(compresult_t cr);
static compresult_t compute_abs(compresult_t cr);
static compresult_t compute_dfunction(const ast_node_t *node, varenv_t *env);
static compresult_t compute_pow(compresult_t lres, compresult_t rres);
static compresult_t compute_multiply(compresult_t lres, compresult_t rres);
static compresult_t compute_divide(compresult_t lres, compresult_t rres);
static compresult_t compute_add(compresult_t lres, compresult_t rres);
static compresult_t compute_subtract(compresult_t lres, compresult_t rres);

compresult_t compute_node(const struct ast_node *node, varenv_t *env)
{
	if (!node) return COMRES_VD(NAN);

	if (node->type == NODE_NUMBER)
		return COMRES_V(number_copy(node->number));
	else if (node->type == NODE_PREVANSWER)
		return get_previous_answer();
	else if (node->type == NODE_VARIABLE)
		return compute_variable(node, env);
	else if (node->type == NODE_LFUNCTION)
		return node->lfunc->logic(node->left, env);
	else if (node->type == NODE_DFUNCTION)
		return compute_dfunction(node, env);

	compresult_t lres = COMRES_E(-1);
	if (node->left) lres = compute_node(node->left, env);
	if (lres.error) return lres;
	if (lres.value.type == NUMBER_DOUBLE && isnan(lres.value.doble))
		return COMRES_VD(NAN);

	if (node->type == NODE_NEGATE)
		return compute_negate(lres);
	else if (node->type == NODE_ABS)
		return compute_abs(lres);
	else if (node->type == NODE_SFUNCTION)
	{
		compresult_t arg = convert_to_double(lres);
		if (arg.error) return arg;
		else return COMRES_VD(node->sfunc->logic(arg.value.doble));
	}
	
	compresult_t rres = COMRES_E(-1);
	if (node->right) rres = compute_node(node->right, env);
	if (rres.error)
	{
		number_free(lres.value);
		return rres;
	}
	if (rres.value.type == NUMBER_DOUBLE && isnan(rres.value.doble))
	{
		number_free(lres.value);
		return COMRES_VD(NAN);
	}

	if (node->type == NODE_POW)
		return compute_pow(lres, rres);
	else if (node->type == NODE_MULTIPLY)
		return compute_multiply(lres, rres);
	else if (node->type == NODE_DIVIDE)
		return compute_divide(lres, rres);
	else if (node->type == NODE_ADD)
		return compute_add(lres, rres);
	else if (node->type == NODE_SUBTRACT)
		return compute_subtract(lres, rres);

	number_free(lres.value);
	number_free(rres.value);
	return COMRES_E(-1);
}

/* helpers */
static compresult_t compute_variable(const ast_node_t *node, varenv_t *env)
{
	while (env)
	{
		if (ident_eq(env->variable, node->ident))
		{
			compresult_t er = env->result;
			return (compresult_t) { .error = er.error, .value = number_copy(er.value) };
		}
		env = env->next;
	}
	const ast_node_t *v = get_variable(node->ident);
	return v ? compute_node(v, env) : COMRES_E(ERROR_COMPUTE_UNDEFINED_VARIABLE);
}
static compresult_t compute_negate(compresult_t cr)
{
	if (cr.error) return cr;

	if (cr.value.type == NUMBER_DOUBLE)
		return COMRES_VD(-cr.value.doble);
	else if (cr.value.type == NUMBER_BIGINT)
	{
		bi_negate(&cr.value.bint, &cr.value.bint);
		return cr;
	}
	else return COMRES_E(ERROR_COMPUTE_ILLEGAL_NUMBER_TYPE);
}
static compresult_t compute_abs(compresult_t cr)
{
	if (cr.error) return cr;

	if (cr.value.type == NUMBER_DOUBLE)
		return COMRES_VD(fabs(cr.value.doble));
	else if (cr.value.type == NUMBER_BIGINT)
	{
		bi_abs(&cr.value.bint, &cr.value.bint);
		return cr;
	}
	else return COMRES_E(ERROR_COMPUTE_ILLEGAL_NUMBER_TYPE);
}
static compresult_t compute_dfunction(const ast_node_t *node, varenv_t *oldenv)
{
	const dfunc_t *dfunc = get_dfunc(node->ident);
	if (!dfunc) return COMRES_E(-1);

	varenv_t *env = NULL;
	const ast_node_t *args = node->left;
	const ast_node_t *pattern = dfunc->args;
	while (pattern)
	{
		if (!args)
		{
			collapse_env(env);
			return COMRES_E(ERROR_COMPUTE_TOO_FEW_ARGUMENTS);
		}

		varenv_t *newenv = calloc(1, sizeof(varenv_t));
		if (!newenv) PANIC("Undefined unallocation");

		newenv->variable = pattern->left->ident;
		newenv->result = compute_node(args->left, oldenv);
		newenv->next = env;
		env = newenv;

		if (newenv->result.error)
		{
			compresult_t cr = newenv->result;
			collapse_env(env);
			return cr;
		}

		pattern = pattern->right;
		args = args->right;
	}

	compresult_t res = compute_node(dfunc->impl, env);
	collapse_env(env);
	return res;
}
static compresult_t compute_pow(compresult_t lres, compresult_t rres)
{
	if (lres.value.type == NUMBER_BIGINT && rres.value.type == NUMBER_BIGINT)
	{
		bi_pow(&lres.value.bint, &lres.value.bint, &rres.value.bint, NULL);
		bi_free(&rres.value.bint);
		return lres;
	}

	compresult_t base = convert_to_double(lres), exp = convert_to_double(rres);
	if (base.error) return base;
	if (exp.error) return exp;
	return COMRES_VD(pow(base.value.doble, exp.value.doble));
}
static compresult_t compute_multiply(compresult_t lres, compresult_t rres)
{
	if (lres.value.type == NUMBER_BIGINT && rres.value.type == NUMBER_BIGINT)
	{
		bi_mul(&lres.value.bint, &lres.value.bint, &rres.value.bint);
		bi_free(&rres.value.bint);
		return lres;
	}

	compresult_t a = convert_to_double(lres), b = convert_to_double(rres);
	if (a.error) return a;
	if (b.error) return b;
	return COMRES_VD(a.value.doble * b.value.doble);
}
static compresult_t compute_divide(compresult_t lres, compresult_t rres)
{
	compresult_t a = convert_to_double(lres), b = convert_to_double(rres);
	if (a.error) return a;
	if (b.error) return b;
	return COMRES_VD(a.value.doble / b.value.doble);
}
static compresult_t compute_add(compresult_t lres, compresult_t rres)
{
	if (lres.value.type == NUMBER_BIGINT && rres.value.type == NUMBER_BIGINT)
	{
		bi_add(&lres.value.bint, &lres.value.bint, &rres.value.bint);
		bi_free(&rres.value.bint);
		return lres;
	}

	compresult_t a = convert_to_double(lres), b = convert_to_double(rres);
	if (a.error) return a;
	if (b.error) return b;
	return COMRES_VD(a.value.doble + b.value.doble);
}
static compresult_t compute_subtract(compresult_t lres, compresult_t rres)
{
	if (lres.value.type == NUMBER_BIGINT && rres.value.type == NUMBER_BIGINT)
	{
		bi_sub(&lres.value.bint, &lres.value.bint, &rres.value.bint);
		bi_free(&rres.value.bint);
		return lres;
	}

	compresult_t a = convert_to_double(lres), b = convert_to_double(rres);
	if (a.error) return a;
	if (b.error) return b;
	return COMRES_VD(a.value.doble - b.value.doble);
}

/* S-function implementation */
double sign(double x) { return x > 0 ? 1 : (x == 0 ? 0 : -1); }
double cot(double x) { return cos(x) / sin(x); }
double coth(double x) { return cosh(x) / sinh(x); }
double acot(double x) { return 1.570796326794896557998981734 - atan(x); }
double acoth(double x) { return atanh(1.0 / x); }

/* L-function implementation */
compresult_t llog(const struct ast_node *argnode, struct varenv *env)
{
	compresult_t base = convert_to_double(compute_node(argnode->left, env));
	if (base.error) return base;
	if (!argnode->right) return COMRES_E(ERROR_COMPUTE_TOO_FEW_ARGUMENTS);
	compresult_t value = convert_to_double(compute_node(argnode->right->left, env));
	if (value.error) return value;

	return COMRES_VD(log(value.value.doble) / log(base.value.doble));
}
compresult_t lmax(const struct ast_node *argnode, struct varenv *env)
{
	double maximum = -INFINITY;
	while (argnode)
	{
		compresult_t cr = convert_to_double(compute_node(argnode->left, env));
		if (cr.error) return cr;
		else
		{
			if (cr.value.doble > maximum) maximum = cr.value.doble;
		}
		argnode = argnode->right;
	}
	return COMRES_VD(maximum);
}
compresult_t lmin(const struct ast_node *argnode, struct varenv *env)
{
	double minimum = INFINITY;
	while (argnode)
	{
		compresult_t cr = convert_to_double(compute_node(argnode->left, env));
		if (cr.error) return COMRES_E(cr.error);
		else
		{
			if (cr.value.doble < minimum) minimum = cr.value.doble;
		}
		argnode = argnode->right;
	}
	return COMRES_VD(minimum);
}
compresult_t lgcd(const struct ast_node *argnode, struct varenv *env)
{
	bigint_t gcd = { 0 };
	bi_init_with_i64(&gcd, 0);

	int err = 0;
	while (argnode)
	{
		compresult_t x = compute_node(argnode->left, env);
		if (x.error)
		{
			number_free(x.value);
			err = x.error;
			goto fail;
		}
		if (x.value.type != NUMBER_BIGINT) 
		{
			err = ERROR_COMPUTE_ILLEGAL_NUMBER_TYPE;
			goto fail;
		}

		if (bi_gcd(&gcd, &gcd, &x.value.bint)) { err = -1; goto fail; }

		argnode = argnode->right;
		number_free(x.value);
	}

	return COMRES_VBI(gcd);

fail:
	bi_free(&gcd);
	return COMRES_E(err);
}
compresult_t llcm(const struct ast_node *argnode, struct varenv *env)
{
	bigint_t lcm = { 0 };
	bi_init_with_i64(&lcm, 1);

	int err = 0;
	while (argnode)
	{
		compresult_t x = compute_node(argnode->left, env);
		if (x.error)
		{
			number_free(x.value);
			err = x.error;
			goto fail;
		}
		if (x.value.type != NUMBER_BIGINT)
		{
			err = ERROR_COMPUTE_ILLEGAL_NUMBER_TYPE;
			goto fail;
		}

		if (bi_lcm(&lcm, &lcm, &x.value.bint)) { err = -1; goto fail; }

		argnode = argnode->right;
		number_free(x.value);
	}

	return COMRES_VBI(lcm);

fail:
	bi_free(&lcm);
	return COMRES_E(err);
}

#define SAFE_EXIT(hld, ret) do { hld = ret; goto exit; } while (0);
compresult_t lmod(const struct ast_node *argnode, struct varenv *env)
{
	compresult_t toret = COMRES_E(-1), dividend = COMRES_E(-1), divisor = COMRES_E(-1);

	dividend = compute_node(argnode->left, env);
	if (dividend.error) SAFE_EXIT(toret, dividend);
	if (dividend.value.type != NUMBER_BIGINT) 
		SAFE_EXIT(toret, COMRES_E(ERROR_COMPUTE_ILLEGAL_NUMBER_TYPE));
	
	if (!argnode->right) SAFE_EXIT(toret, COMRES_E(ERROR_COMPUTE_TOO_FEW_ARGUMENTS));
	divisor = compute_node(argnode->right->left, env);
	if (divisor.error) SAFE_EXIT(toret, divisor);
	if (divisor.value.type != NUMBER_BIGINT)
		SAFE_EXIT(toret, COMRES_E(ERROR_COMPUTE_ILLEGAL_NUMBER_TYPE));
	
	if (bi_is_zero(&divisor.value.bint))
		SAFE_EXIT(toret, COMRES_E(ERROR_COMPUTE_DIVISION_BY_ZERO));

	bigint_t rem = { 0 };
	bi_init(&rem, 1);
	bi_div(NULL, &rem, &dividend.value.bint, &divisor.value.bint);
	if (rem.sign) bi_add(&rem, &rem, &divisor.value.bint);

	toret = COMRES_VBI(rem);

exit:
	number_free(dividend.value);
	number_free(divisor.value);

	return toret;
}
compresult_t lpow(const struct ast_node *argnode, struct varenv *env)
{
	compresult_t toret = COMRES_E(-1), base = COMRES_E(-1), 
		exp = COMRES_E(-1), mod = COMRES_E(-1);

	base = compute_node(argnode->left, env);
	if (base.error) SAFE_EXIT(toret, base);
	if (base.value.type != NUMBER_BIGINT)
		SAFE_EXIT(toret, COMRES_E(ERROR_COMPUTE_ILLEGAL_NUMBER_TYPE));

	if (!argnode->right) SAFE_EXIT(toret, COMRES_E(ERROR_COMPUTE_TOO_FEW_ARGUMENTS));
	exp = compute_node(argnode->right->left, env);
	if (exp.error) SAFE_EXIT(toret, exp);
	if (exp.value.type != NUMBER_BIGINT)
		SAFE_EXIT(toret, COMRES_E(ERROR_COMPUTE_ILLEGAL_NUMBER_TYPE));
	
	if (!argnode->right->right) SAFE_EXIT(toret, COMRES_E(ERROR_COMPUTE_TOO_FEW_ARGUMENTS));
	mod = compute_node(argnode->right->right->left, env);
	if (mod.error) SAFE_EXIT(toret, mod);
	if (mod.value.type != NUMBER_BIGINT)
		SAFE_EXIT(toret, COMRES_E(ERROR_COMPUTE_ILLEGAL_NUMBER_TYPE));

	bigint_t res = { 0 };
	bi_init(&res, 1);
	bi_pow(&res, &base.value.bint, &exp.value.bint, &mod.value.bint);

	toret = COMRES_VBI(res);

exit:
	number_free(base.value);
	number_free(exp.value);
	number_free(mod.value);

	return toret;
}
compresult_t linv(const struct ast_node *argnode, struct varenv *env)
{
	compresult_t toret = COMRES_E(-1), base = COMRES_E(-1), mod = COMRES_E(-1);

	base = compute_node(argnode->left, env);
	if (base.error) SAFE_EXIT(toret, base);
	if (base.value.type != NUMBER_BIGINT)
		SAFE_EXIT(toret, COMRES_E(ERROR_COMPUTE_ILLEGAL_NUMBER_TYPE));

	if (!argnode->right) SAFE_EXIT(toret, COMRES_E(ERROR_COMPUTE_TOO_FEW_ARGUMENTS));
	mod = compute_node(argnode->right->left, env);
	if (mod.error) SAFE_EXIT(toret, mod);
	if (mod.value.type != NUMBER_BIGINT)
		SAFE_EXIT(toret, COMRES_E(ERROR_COMPUTE_ILLEGAL_NUMBER_TYPE));

	bigint_t res = { 0 };
	bi_init(&res, 1);
	int status = bi_inv(&res, &base.value.bint, &mod.value.bint);
	
	toret = status ? COMRES_E(ERROR_COMPUTE_NO_MINV_EXIST) : COMRES_VBI(res);

exit:
	number_free(base.value);
	number_free(mod.value);

	return toret;
}
