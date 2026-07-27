#include "bignum.h"

#include <string.h>
#include <stdlib.h>


#define _BI_MAX(a, b) ((a)->size > (b)->size ? (a) : (b))
#define _BI_MIN(a, b) ((a)->size < (b)->size ? (a) : (b))
#define _BI_MAX_LEN(a, b) _BI_MAX(a, b)->size
#define _BI_MIN_LEN(a, b) _BI_MIN(a, b)->size


/* ------------ INIT & FREE ------------ */

int bi_init(bigint_t *bi, int initsize)
{
	if (!bi) return 1;

	bi->alloc = initsize;
	bi->sign = 0;
	bi->size = 1;
	bi->dlimbs = calloc(bi->alloc, sizeof(uint8_t));
	if (!bi->dlimbs) return 1;
	return 0;
}
void bi_free(bigint_t *bi)
{
	if (!bi) return;

	free(bi->dlimbs);
	bi->dlimbs = NULL;
	bi->sign = 0;
	bi->size = 0;
	bi->alloc = 0;
}
int bi_copy(bigint_t *dest, const bigint_t *src)
{
	if (!dest || !src) return 1;
	if (dest == src) return 0;
	bi_free(dest);
	dest->dlimbs = calloc(src->size, sizeof(uint8_t));
	if (!dest->dlimbs) return 1;
	memcpy(dest->dlimbs, src->dlimbs, src->size * sizeof(uint8_t));
	dest->alloc = src->size;
	dest->size = src->size;
	dest->sign = src->sign;
	return 0;
}
int bi_is_zero(const bigint_t *bi)
{
	return bi->alloc > 0 && bi->size == 1 && bi->dlimbs[0] == 0 && bi->sign == 0;
}


/* ------------ HELPERS ------------ */

static int _bi_resize(bigint_t *bi, int newsize)
{
	if (!bi) return 1;
	uint8_t *newdlimbs = realloc(bi->dlimbs, newsize * sizeof(uint8_t));
	if (!newdlimbs) return 1;
	bi->dlimbs = newdlimbs;
	bi->alloc = newsize;
	if (bi->size > bi->alloc) bi->size = bi->alloc;
	return 0;
}
static void _bi_trim(bigint_t *bi)
{
	bi->size = bi->alloc;
	while (bi->size > 1 && bi->dlimbs[bi->size - 1] == 0) bi->size--;
}
static void _bi_validate_zero(bigint_t *bi)
{
	if (bi->size == 1 && bi->dlimbs[0] == 0)
		bi->sign = 0;
}


/* ------------ STRINGS ------------ */

int bi_init_with_str(bigint_t *bi, const char *str)
{
	if (!bi || !str) return 1;
	int len = (int)strlen(str);
	if (len == 0) return 1;

	bi_init(bi, len);
	bi->sign = str[0] == '-';

	for (int i = 0; i < len; i++)
		bi->dlimbs[i] = str[(len - 1) - i] - '0';

	_bi_trim(bi);
	_bi_validate_zero(bi);
	return 0;
}
int bi_init_with_i64(bigint_t *bi, int64_t x)
{
	if (!bi) return 1;

	bi_init(bi, 20);

	if (x < 0)
	{
		bi->sign = 1;
		x = -x;
	}

	int i = 0;
	while (x)
	{
		bi->dlimbs[i] = x % 10;
		x /= 10;
		i++;
	}

	_bi_trim(bi);
	_bi_validate_zero(bi);
	return 0;
}
char *bi_to_str(const bigint_t *bi)
{
	if (!bi) return NULL;

	char *str = calloc(bi->size + (bi->sign ? 1 : 0) + 1, sizeof(char));
	if (!str) return NULL;

	int off = 0;
	if (bi->sign)
	{
		str[0] = '-';
		off = 1;
	}

	for (int i = 0; i < bi->size; i++)
		str[i + off] = bi->dlimbs[(bi->size - 1) - i] + '0';
	str[off + bi->size] = '\0';

	return str;
}
double bi_as_double(const bigint_t *bi)
{
	double res = 0.0;
	double power = 1.0;
	for (int i = 0; i < bi->size; i++)
	{
		res += (double)bi->dlimbs[i] * power;
		power *= 10.0;
	}
	return res;
}


/* ------------ ARITHMETIC ------------ */

static int _bi_cmp_abs(const bigint_t *a, const bigint_t *b)
{
	if (a->size > b->size) return 1;
	if (a->size < b->size) return -1;
	
	int s = a->size;
	for (int i = s - 1; i >= 0; i--)
	{
		if (a->dlimbs[i] > b->dlimbs[i]) return 1;
		if (a->dlimbs[i] < b->dlimbs[i]) return -1;
	}
	return 0;
}
int bi_cmp(const bigint_t *a, const bigint_t *b)
{
	if (!a || !b) return 0;

	if (a->sign != b->sign)
		return a->sign ? -1 : 1;
	
	int c = _bi_cmp_abs(a, b);
	return a->sign ? -c : c;
}

int bi_negate(bigint_t *r, const bigint_t *x)
{
	if (bi_copy(r, x)) return 1;
	r->sign = !x->sign;
	_bi_validate_zero(r);
	return 0;
}
int bi_abs(bigint_t *r, const bigint_t *x)
{
	if (bi_copy(r, x)) return 1;
	r->sign = 0;
	return 0;
}
static int _bi_add_unsigned(bigint_t *r, const bigint_t *a, const bigint_t *b)
{
	int maxlen = _BI_MAX_LEN(a, b), minlen = _BI_MIN_LEN(a, b);
	int reslen = maxlen + 1;
	if (reslen > r->alloc)
	{
		if (_bi_resize(r, reslen)) return 1;
	}

	int carry = 0;
	int i = 0;
	for (; i < minlen; i++)
	{
		int d = a->dlimbs[i] + b->dlimbs[i] + carry;
		if (d >= 10)
		{
			d -= 10;
			carry = 1;
		}
		else carry = 0;
		r->dlimbs[i] = (uint8_t)d;
	}

	const bigint_t *longer = _BI_MAX(a, b);
	for (; i < maxlen; i++)
	{
		int d = longer->dlimbs[i] + carry;
		if (d >= 10)
		{
			d -= 10;
			carry = 1;
		}
		else carry = 0;
		r->dlimbs[i] = (uint8_t)d;
	}

	r->dlimbs[i] = (uint8_t)carry;
	_bi_trim(r);
	return 0;
}
/* a must be >= than b */
static int _bi_sub_unsigned(bigint_t *r, const bigint_t *a, const bigint_t *b)
{
	int maxlen = _BI_MAX_LEN(a, b), minlen = _BI_MIN_LEN(a, b);
	if (maxlen > r->alloc)
	{
		if (_bi_resize(r, maxlen)) return 1;
	}

	int borrow = 0;
	int i = 0;
	for (; i < minlen; i++)
	{
		int d = a->dlimbs[i] - b->dlimbs[i] - borrow;
		if (d < 0)
		{
			d += 10;
			borrow = 1;
		}
		else borrow = 0;
		r->dlimbs[i] = (uint8_t)d;
	}

	for (; i < maxlen; i++)
	{
		int d = a->dlimbs[i] - borrow;
		if (d < 0)
		{
			d += 10;
			borrow = 1;
		}
		else borrow = 0;
		r->dlimbs[i] = (uint8_t)d;
	}

	_bi_trim(r);
	return 0;
}

int bi_add(bigint_t *sum, const bigint_t *a, const bigint_t *b)
{
	if (!sum || !a || !b) return 1;

	if (a->sign == b->sign)
	{
		if (_bi_add_unsigned(sum, a, b)) return 1;
		sum->sign = a->sign;
		return 0;
	}
	
	int c = _bi_cmp_abs(a, b);
	if (c == 0)   /* |a| = |b| */
	{
		if (_bi_resize(sum, 1)) return 1;
		sum->dlimbs[0] = 0;
	}
	else if (c == -1)   /* |a| < |b|: -A + B or A + -B */
	{
		if (_bi_sub_unsigned(sum, b, a)) return 1;
		sum->sign = b->sign;
	}
	else   /* |a| > |b|: A + -B or -A + B */
	{
		if (_bi_sub_unsigned(sum, a, b)) return 1;
		sum->sign = a->sign;
	}

	_bi_trim(sum);
	_bi_validate_zero(sum);
	return 0;
}
int bi_sub(bigint_t *diff, const bigint_t *a, const bigint_t *b)
{
	if (!diff || !a || !b) return 1;

	/* -A - B [-(A + B)] or A - -B [A + B] */
	if (a->sign != b->sign)
	{
		if (_bi_add_unsigned(diff, a, b)) return 1;
		diff->sign = a->sign;
		return 0;
	}

	int c = _bi_cmp_abs(a, b);
	if (c == 0)   /* |a| = |b| */
	{
		if (_bi_resize(diff, 1)) return 1;
		diff->dlimbs[0] = 0;
	}
	else if (c == -1)  /* |a| < |b|: -A - -B [-(A - B)] or A - B */
	{
		if (_bi_sub_unsigned(diff, b, a)) return 1;
		diff->sign = !b->sign;
	}
	else   /* |a| > |b|: -A - -B [-(A - B)] or A - B */
	{
		if (_bi_sub_unsigned(diff, a, b)) return 1;
		diff->sign = a->sign;
	}

	_bi_trim(diff);
	_bi_validate_zero(diff);
	return 0;
}

int bi_mul(bigint_t *prod, const bigint_t *a, const bigint_t *b)
{
	if (!prod || !a || !b) return 1;

	if (bi_is_zero(a) || bi_is_zero(b))
	{
		bi_free(prod);
		bi_init_with_i64(prod, 0);
		return 0;
	}

	int reslen = a->size + b->size;

	uint8_t *temp = calloc(reslen, sizeof(uint8_t));
	if (!temp) return 1;

	for (int i = 0; i < a->size; i++)
	{
		int carry = 0;
		for (int j = 0; j < b->size; j++)
		{
			int d = temp[i + j] + (a->dlimbs[i] * b->dlimbs[j]) + carry;
			temp[i + j] = (d % 10);
			carry = d / 10;
		}

		int k = b->size;
		while (carry > 0)
		{
			int d = temp[i + k] + carry;
			temp[i + k] = d % 10;
			carry = d / 10;
			k++;
		}
	}

	int sign = a->sign ^ b->sign;
	bi_free(prod);
	prod->dlimbs = temp;
	prod->alloc = reslen;
	prod->sign = sign;
	_bi_trim(prod);
	return 0;
}
/* long division for base 10 (a.k.a. schoolbook method) */
int bi_div(bigint_t *quot, bigint_t *rem, const bigint_t *a, const bigint_t *b)
{
	if (!a || !b) return 1;

	if (bi_is_zero(b)) return 1;

	int abscmp = _bi_cmp_abs(a, b);
	if (abscmp == -1)
	{
		if (quot)
		{
			bi_free(quot);
			bi_init_with_i64(quot, 0);
		}
		if (rem)
		{
			bigint_t tr = { 0 };
			if (bi_copy(&tr, a)) return 1;
			bi_free(rem);
			*rem = tr;
		}

		return 0;
	}

	bigint_t tq = { 0 }, tr = { 0 };
	if (bi_init(&tq, a->size) || bi_init(&tr, a->size + 1))
	{
		bi_free(&tq);
		bi_free(&tr);
		return 1;
	}
	
	tr.dlimbs[0] = 0;

	for (int i = a->size - 1; i >= 0; i--)
	{
		if (!bi_is_zero(&tr))
		{
			memmove(tr.dlimbs + 1, tr.dlimbs, tr.size * sizeof(uint8_t));
			tr.size++;
		}

		tr.dlimbs[0] = a->dlimbs[i];

		int d = 0;
		while (_bi_cmp_abs(&tr, b) >= 0)
		{
			_bi_sub_unsigned(&tr, &tr, b);
			d++;
		}
		tq.dlimbs[i] = (uint8_t)d;
	}

	_bi_trim(&tq); _bi_trim(&tr);
	_bi_validate_zero(&tq); _bi_validate_zero(&tr);
	tq.sign = a->sign ^ b->sign;
	tr.sign = a->sign;

	if (quot)
	{
		bi_free(quot);
		*quot = tq;
	}
	else bi_free(&tq);
	if (rem)
	{
		bi_free(rem);
		*rem = tr;
	}
	else bi_free(&tr);

	return 0;
}

static int _bi_div2_and_was_odd(bigint_t *bi)
{
	int carry = 0;
	for (int i = bi->size - 1; i >= 0; i--)
	{
		int current = carry * 10 + bi->dlimbs[i];
		bi->dlimbs[i] = (uint8_t)(current / 2);
		carry = current % 2;
	}
	_bi_trim(bi);
	return carry == 1;
}
int bi_pow(bigint_t *res, const bigint_t *base, const bigint_t *exp, const bigint_t *mod)
{
	if (!res || !base || !exp) return 1;
	if (exp->sign) return 1;
	if (mod && bi_is_zero(mod)) return 1;

	/* 0^anything = 0, (even 0^0 = 0) */
	if (bi_is_zero(base))
	{
		bi_free(res);
		bi_init_with_i64(res, 0);
		return 0;
	}

	bigint_t tb = { 0 }, te = { 0 }, acc = { 0 };
	if (bi_copy(&tb, base) || bi_copy(&te, exp) || bi_init_with_i64(&acc, 1))
		goto fail;

	if (mod)
	{
		if (bi_div(NULL, &tb, &tb, mod)) goto fail;
	}

	while (!bi_is_zero(&te))
	{
		if (_bi_div2_and_was_odd(&te))
		{
			if (bi_mul(&acc, &acc, &tb)) goto fail;
			if (mod)
			{
				if (bi_div(NULL, &acc, &acc, mod)) goto fail;
			}
		}
		if (!bi_is_zero(&te))
		{
			if (bi_mul(&tb, &tb, &tb)) goto fail;
			if (mod)
			{
				if (bi_div(NULL, &tb, &tb, mod)) goto fail;
			}
		}
	}

	bi_free(res);
	*res = acc;
	bi_free(&tb); bi_free(&te);
	return 0;

fail:
	bi_free(&tb); bi_free(&te); bi_free(&acc);
	return 1;
}


/* ------------- ADDITIONAL ALGORITHMS ------------- */

int bi_gcd(bigint_t *gcd, const bigint_t *a, const bigint_t *b)
{
	if (!gcd || !a || !b) return 1;

	bigint_t r0 = { 0 }, r1 = { 0 }, tmp = { 0 };

	if (bi_copy(&r0, a) || bi_copy(&r1, b) || bi_init(&tmp, 1))
		goto fail;

	r0.sign = r1.sign = 0;

	while (!bi_is_zero(&r1))
	{
		/* tmp = r0 % r1 */
		if (bi_div(NULL, &tmp, &r0, &r1)) goto fail;
		/* ( r0, r1 ) = ( r1, r0 % r1 ) */
		if (bi_copy(&r0, &r1)) goto fail;
		if (bi_copy(&r1, &tmp)) goto fail;
	}

	bi_free(gcd);
	*gcd = r0;
	bi_free(&tmp); bi_free(&r1);
	return 0;

fail:
	bi_free(&r0); bi_free(&r1); bi_free(&tmp);
	return 1;
}

/* No idea why it's here */
int bi_gcdext(bigint_t *gcd, const bigint_t *a, bigint_t *s, const bigint_t *b, bigint_t *t)
{
	if (!a || !b) return 1;

	bigint_t r0 = { 0 }, r1 = { 0 };  /* Remainders */
	bigint_t s0 = { 0 }, s1 = { 0 };  /* S-coeff */
	bigint_t t0 = { 0 }, t1 = { 0 };  /* T-coeff */
	bigint_t q = { 0 }, tmp1 = { 0 }, tmp2 = { 0 };

	if (
		bi_copy(&r0, a) || bi_copy(&r1, b) ||
		bi_init_with_i64(&s0, 1) || bi_init_with_i64(&s1, 0) ||
		bi_init_with_i64(&t0, 0) || bi_init_with_i64(&t1, 1) ||
		bi_init(&q, 1) || bi_init(&tmp1, 1) || bi_init(&tmp2, 1)
	) goto fail;

	r0.sign = r1.sign = 0;

	while (!bi_is_zero(&r1))
	{
		/* q = r0 / r1 ; tmp1 = r0 % r1 */
		if (bi_div(&q, &tmp1, &r0, &r1)) goto fail;

		/* ( r0, r1 ) = ( r1, r0 % r1 ) */
		if (bi_copy(&r0, &r1)) goto fail;
		if (bi_copy(&r1, &tmp1)) goto fail;

		/* ( s0, s1 ) = ( s1, s0 - q * s1 ) */
		if (bi_mul(&tmp2, &q, &s1)) goto fail;
		if (bi_sub(&tmp2, &s0, &tmp2)) goto fail;
		if (bi_copy(&s0, &s1)) goto fail;
		if (bi_copy(&s1, &tmp2)) goto fail;

		/* ( t0, t1 ) = ( t1, t0 - q * t1 ) */
		if (bi_mul(&tmp2, &q, &t1)) goto fail;
		if (bi_sub(&tmp2, &t0, &tmp2)) goto fail;
		if (bi_copy(&t0, &t1)) goto fail;
		if (bi_copy(&t1, &tmp2)) goto fail;
	}

	/* gcd = r0, s = s0, t = t0 */
	if (gcd)
	{
		bi_free(gcd);
		*gcd = r0;
	}
	else bi_free(&r0);
	if (s)
	{
		s0.sign ^= a->sign;
		_bi_validate_zero(&s0);
		bi_free(s);
		*s = s0;
	}
	else bi_free(&s0);
	if (t)
	{
		t0.sign ^= b->sign;
		_bi_validate_zero(&t0);
		bi_free(t);
		*t = t0;
	}
	else bi_free(&t0);

	bi_free(&r1); bi_free(&s1); bi_free(&t1);
	bi_free(&q); bi_free(&tmp1); bi_free(&tmp2);
	return 0;

fail:
	bi_free(&r0); bi_free(&r1);
	bi_free(&s0); bi_free(&s1);
	bi_free(&t0); bi_free(&t1);
	bi_free(&q); bi_free(&tmp1); bi_free(&tmp2);
	return 1;
}

int bi_lcm(bigint_t *lcm, const bigint_t *a, const bigint_t *b)
{
	if (!lcm || !a || !b) return 1;

	if (bi_is_zero(a) || bi_is_zero(b))
	{
		bi_free(lcm);
		bi_init_with_i64(lcm, 0);
		return 0;
	}

	bigint_t gcd = { 0 }, prod = { 0 };
	if (bi_init(&gcd, 1) || bi_init(&prod, 1))
		goto fail;

	if (bi_gcd(&gcd, a, b)) goto fail;
	if (bi_div(&prod, NULL, a, &gcd)) goto fail;
	if (bi_mul(&prod, &prod, b)) goto fail;

	prod.sign = 0;
	bi_free(lcm);
	*lcm = prod;
	bi_free(&gcd);
	return 0;

fail:
	bi_free(&gcd); bi_free(&prod);
	return 1;
}

int bi_inv(bigint_t *inv, const bigint_t *a, const bigint_t *m)
{
	if (!inv || !a || !m) return 1;
	if (bi_is_zero(a) || bi_is_zero(m)) return 1;

	bigint_t r0 = { 0 }, r1 = { 0 };
	bigint_t t0 = { 0 }, t1 = { 0 };
	bigint_t q = { 0 }, tmp = { 0 };

	if (
		bi_copy(&r0, m) || bi_copy(&r1, a) ||
		bi_init_with_i64(&t0, 0) || bi_init_with_i64(&t1, 1) ||
		bi_init(&q, 1) || bi_init(&tmp, 1)
	) goto fail;

	while (!bi_is_zero(&r1))
	{
		/* q = r0 / r1 ; tmp = r0 % r1 */
		if (bi_div(&q, &tmp, &r0, &r1)) goto fail;

		/* ( r0, r1 ) = ( r1, r0 % r1 ) */
		if (bi_copy(&r0, &r1)) goto fail;
		if (bi_copy(&r1, &tmp)) goto fail;

		/* ( t0, t1 ) = ( t1, t0 - q * t1 ) */
		if (bi_mul(&tmp, &q, &t1)) goto fail;
		if (bi_sub(&tmp, &t0, &tmp)) goto fail;
		if (bi_copy(&t0, &t1)) goto fail;
		if (bi_copy(&t1, &tmp)) goto fail;
	}

	if (!(r0.size == 1 && r0.dlimbs[0] == 1)) goto fail;  /* no inverse found */
	
	if (t0.sign)
	{
		if (bi_add(&t0, &t0, m)) goto fail;
	}

	bi_free(inv);
	*inv = t0;

	bi_free(&r0); bi_free(&r1);  bi_free(&t1);
	bi_free(&q); bi_free(&tmp);
	return 0;

fail:
	bi_free(&r0); bi_free(&r1);
	bi_free(&t0); bi_free(&t1);
	bi_free(&q); bi_free(&tmp);
	return 1;
}
