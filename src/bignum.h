#pragma once

#include <stdint.h>

/* -------------- INTEGERS -------------- */

typedef struct
{
	/* 0 - positive or zero, 1 - negative */
	int sign;
	int alloc;
	int size;
	uint8_t *dlimbs;
} bigint_t;

int bi_init(bigint_t *bi, int initsize);
void bi_free(bigint_t *bi);
int bi_copy(bigint_t *dest, const bigint_t *src);

int bi_init_with_str(bigint_t *bi, const char *str);
int bi_init_with_i64(bigint_t *bi, int64_t x);
char *bi_to_str(const bigint_t *bi);
double bi_as_double(const bigint_t *bi);

int bi_cmp(const bigint_t *a, const bigint_t *b);
int bi_is_zero(const bigint_t *bi);

int bi_negate(bigint_t *r, const bigint_t *x);
int bi_abs(bigint_t *r, const bigint_t *x);
int bi_add(bigint_t *sum, const bigint_t *a, const bigint_t *b);
int bi_sub(bigint_t *diff, const bigint_t *a, const bigint_t *b);
int bi_mul(bigint_t *prod, const bigint_t *a, const bigint_t *b);
int bi_div(bigint_t *quot /* nullable */, bigint_t *rem /* nullable */,
	const bigint_t *a, const bigint_t *b);
int bi_pow(bigint_t *res, const bigint_t *base, const bigint_t *exp,
	const bigint_t *mod /* nullable */);

/* Addition algorithms */
int bi_gcd(bigint_t *gcd, const bigint_t *a, const bigint_t *b);
int bi_gcdext(bigint_t *gcd /* nullable */, const bigint_t *a,
	bigint_t *s /* nullable */, const bigint_t *b, bigint_t *t /* nullable */);
int bi_lcm(bigint_t *lcm, const bigint_t *a, const bigint_t *b);
int bi_inv(bigint_t *inv, const bigint_t *a, const bigint_t *m);
