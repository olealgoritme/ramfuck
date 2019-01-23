#define _DEFAULT_SOURCE /* for snprintf(3) */
#include "value.h"

#include <memory.h>
#include <stdio.h>

int value_init(struct value *dest, enum value_type type, void *pvalue)
{
    dest->type = type;
    memcpy(&dest->data, pvalue, value_sizeof(dest));
    return 1;
}

int value_init_s8(struct value *dest, int8_t value)
{
    dest->type = S8;
    dest->data.s8 = value;
    return 1;
}

int value_init_u8(struct value *dest, uint8_t value)
{
    dest->type = U8;
    dest->data.u8 = value;
    return 1;
}

int value_init_s16(struct value *dest, int16_t value)
{
    dest->type = S16;
    dest->data.s16 = value;
    return 1;
}

int value_init_u16(struct value *dest, uint16_t value)
{
    dest->type = U16;
    dest->data.u16 = value;
    return 1;
}

int value_init_s32(struct value *dest, int32_t value)
{
    dest->type = S32;
    dest->data.s32 = value;
    return 1;
}

int value_init_u32(struct value *dest, uint32_t value)
{
    dest->type = U32;
    dest->data.u32 = value;
    return 1;
}

int value_init_s64(struct value *dest, int64_t value)
{
    dest->type = S64;
    dest->data.s64 = value;
    return 1;
}

int value_init_u64(struct value *dest, uint64_t value)
{
    dest->type = U64;
    dest->data.u64 = value;
    return 1;
}

int value_init_f32(struct value *dest, float value)
{
    dest->type = F32;
    dest->data.f32 = value;
    return 1;
}

int value_init_f64(struct value *dest, double value)
{
    dest->type = F64;
    dest->data.f64 = value;
    return 1;
}

int value_is_zero(const struct value *dest)
{
    size_t i, j;
    for (i = 0, j = value_sizeof(dest); i < j; i++) {
        if (((char *)&dest->data)[i] != 0)
            return 0;
    }
    return 1;
}

const char *value_type_to_string(enum value_type type)
{
    switch (type)
    {
    case SINT: return "sint";
    case UINT: return "uint";
    case FPU:  return "fpu";

    case S8:  return "s8";
    case S16: return "s16";
    case S32: return "s32";
    case S64: return "s64";

    case U8:  return "u8";
    case U16: return "u16";
    case U32: return "u32";
    case U64: return "u64";

    case F32: return "f32";
    case F64: return "f64";

    default: break;
    }
    return "void";
}

size_t value_to_string(const struct value *value, char *out, size_t size)
{
    switch (value->type)
    {
    case S8:  return snprintf(out, size, "%ld", (long)value->data.s8);
    case S16: return snprintf(out, size, "%ld", (long)value->data.s16);
    case S32: return snprintf(out, size, "%ld", (long)value->data.s32);
    case S64: return snprintf(out, size, "%ld", (long)value->data.s64);

    case U8:  return snprintf(out, size, "%lu", (unsigned long)value->data.u8);
    case U16: return snprintf(out, size, "%lu", (unsigned long)value->data.u16);
    case U32: return snprintf(out, size, "%lu", (unsigned long)value->data.u32);
    case U64: return snprintf(out, size, "%lu", (unsigned long)value->data.u64);

    case F32: return snprintf(out, size, "%g", value->data.f32);
    case F64: return snprintf(out, size, "%g", value->data.f64);

    default: break;
    }
    return snprintf(out, size, "???");
}

enum value_type value_type_from_string(const char *str)
{
    return value_type_from_substring(str, strlen(str));
}

enum value_type value_type_from_substring(const char *str, size_t len)
{
    if (!memcmp(str, "s8", len)) {
        return S8;
    } else if (!memcmp(str, "s16", len)) {
        return S16;
    } else if (!memcmp(str, "s32", len)) {
        return S32;
    } else if (!memcmp(str, "s64", len)) {
        return S64;
    } else if (!memcmp(str, "u8", len)) {
        return U8;
    } else if (!memcmp(str, "u16", len)) {
        return U16;
    } else if (!memcmp(str, "u32", len)) {
        return U32;
    } else if (!memcmp(str, "u64", len)) {
        return U64;
    } else if (!memcmp(str, "f32", len)) {
        return F32;
    } else if (!memcmp(str, "f64", len)) {
        return F64;
    }

    return 0;
}

/*
 * Dummy no-operation & promotion methods.
 */
static int dummy_nop(struct value *op1, struct value *op2, struct value *out)
{
    return 0; /* error */
}

static int dummy_unop(struct value *op1, struct value *out)
{
    return 0; /* error */
}

#define warning(x) fprintf(stderr, "%s\n", x)

static int dummy_neg(struct value *op1, struct value *out)
{
    warning("value: dummy_neg called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->neg(out, out);
}

static int dummy_not(struct value *op1, struct value *out)
{
    warning("value: dummy_not called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->not(out, out);
}

static int dummy_compl(struct value *op1, struct value *out)
{
    warning("value: dummy_compl called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->compl(out, out);
}

static int dummy_add(struct value *op1, struct value *op2, struct value *out)
{
    warning("value: dummy_add called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->add(out, op2, out);
}

static int dummy_sub(struct value *op1, struct value *op2, struct value *out)
{
    warning("value: dummy_sub called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->sub(out, op2, out);
}

static int dummy_mul(struct value *op1, struct value *op2, struct value *out)
{
    warning("value: dummy_mul called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->mul(out, op2, out);
}

static int dummy_div(struct value *op1, struct value *op2, struct value *out)
{
    warning("value: dummy_div called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->div(out, op2, out);
}

static int dummy_mod(struct value *op1, struct value *op2, struct value *out)
{
    warning("value: dummy_mod called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->mod(out, op2, out);
}

static int dummy_and(struct value *op1, struct value *op2, struct value *out)
{
    warning("value: dummy_and called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->and(out, op2, out);
}
static int dummy_xor(struct value *op1, struct value *op2, struct value *out)
{
    warning("value: dummy_xor called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->xor(out, op2, out);
}

static int dummy_or(struct value *op1, struct value *op2, struct value *out)
{
    warning("value: dummy_or called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->or(out, op2, out);
}

static int dummy_shl(struct value *op1, struct value *op2, struct value *out)
{
    warning("value: dummy_shl called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->shl(out, op2, out);
}

static int dummy_shr(struct value *op1, struct value *op2, struct value *out)
{
    warning("value: dummy_shr called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->shr(out, op2, out);
}

static int dummy_eq(struct value *op1, struct value *op2, struct value *out)
{
    warning("value: dummy_eq called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->eq(out, op2, out);
}

static int dummy_neq(struct value *op1, struct value *op2, struct value *out)
{
    warning("value: dummy_neq called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->neq(out, op2, out);
}

static int dummy_lt(struct value *op1, struct value *op2, struct value *out)
{
    warning("value: dummy_lt called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->lt(out, op2, out);
}

static int dummy_gt(struct value *op1, struct value *op2, struct value *out)
{
    warning("value: dummy_gt called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->gt(out, op2, out);
}

static int dummy_le(struct value *op1, struct value *op2, struct value *out)
{
    warning("value: dummy_le called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->le(out, op2, out);
}

static int dummy_ge(struct value *op1, struct value *op2, struct value *out)
{
    warning("value: dummy_ge called");
    value_ops(op1)->cast_to_s32(op1, out);
    return value_ops(out)->ge(out, op2, out);
}

/*
 * s8 methods.
 */
static int s8_to_s8(struct value *this, struct value *out)
{
    return value_init_s8(out, this->data.s8);
}

static int s8_to_u8(struct value *this, struct value *out)
{
    return value_init_u8(out, this->data.s8);
}

static int s8_to_s16(struct value *this, struct value *out)
{
    return value_init_s16(out, this->data.s8);
}

static int s8_to_u16(struct value *this, struct value *out)
{
    return value_init_u16(out, this->data.s8);
}

static int s8_to_s32(struct value *this, struct value *out)
{
    return value_init_s32(out, this->data.s8);
}

static int s8_to_u32(struct value *this, struct value *out)
{
    return value_init_u32(out, this->data.s8);
}

static int s8_to_s64(struct value *this, struct value *out)
{
    return value_init_s64(out, this->data.s8);
}

static int s8_to_u64(struct value *this, struct value *out)
{
    return value_init_u64(out, this->data.s8);
}

static int s8_to_f32(struct value *this, struct value *out)
{
    return value_init_f32(out, this->data.s8);
}

static int s8_to_f64(struct value *this, struct value *out)
{
    return value_init_f64(out, this->data.s8);
}

static int s8_assign(struct value *this, struct value *src)
{
    return value_ops(src)->cast_to_s8(src, this);
}

/*
 * u8 methods.
 */
static int u8_to_s8(struct value *this, struct value *out)
{
    return value_init_s8(out, this->data.u8);
}

static int u8_to_u8(struct value *this, struct value *out)
{
    return value_init_u8(out, this->data.u8);
}

static int u8_to_s16(struct value *this, struct value *out)
{
    return value_init_s16(out, this->data.u8);
}

static int u8_to_u16(struct value *this, struct value *out)
{
    return value_init_u16(out, this->data.u8);
}

static int u8_to_s32(struct value *this, struct value *out)
{
    return value_init_s32(out, this->data.u8);
}

static int u8_to_u32(struct value *this, struct value *out)
{
    return value_init_u32(out, this->data.u8);
}

static int u8_to_s64(struct value *this, struct value *out)
{
    return value_init_s64(out, this->data.u8);
}

static int u8_to_u64(struct value *this, struct value *out)
{
    return value_init_u64(out, this->data.u8);
}

static int u8_to_f32(struct value *this, struct value *out)
{
    return value_init_f32(out, this->data.u8);
}

static int u8_to_f64(struct value *this, struct value *out)
{
    return value_init_f64(out, this->data.u8);
}

static int u8_assign(struct value *this, struct value *src)
{
    return value_ops(src)->cast_to_u8(src, this);
}

/*
 * s16 methods.
 */
static int s16_to_s8(struct value *this, struct value *out)
{
    return value_init_s8(out, this->data.s16);
}

static int s16_to_u8(struct value *this, struct value *out)
{
    return value_init_u8(out, this->data.s16);
}

static int s16_to_s16(struct value *this, struct value *out)
{
    return value_init_s16(out, this->data.s16);
}

static int s16_to_u16(struct value *this, struct value *out)
{
    return value_init_u16(out, this->data.s16);
}

static int s16_to_s32(struct value *this, struct value *out)
{
    return value_init_s32(out, this->data.s16);
}

static int s16_to_u32(struct value *this, struct value *out)
{
    return value_init_u32(out, this->data.s16);
}

static int s16_to_s64(struct value *this, struct value *out)
{
    return value_init_s64(out, this->data.s16);
}

static int s16_to_u64(struct value *this, struct value *out)
{
    return value_init_u64(out, this->data.s16);
}

static int s16_to_f32(struct value *this, struct value *out)
{
    return value_init_f32(out, this->data.s16);
}

static int s16_to_f64(struct value *this, struct value *out)
{
    return value_init_f64(out, this->data.s16);
}

static int s16_assign(struct value *this, struct value *src)
{
    return value_ops(src)->cast_to_s16(src, this);
}

/*
 * u16 methods.
 */
static int u16_to_s8(struct value *this, struct value *out)
{
    return value_init_s8(out, this->data.u16);
}

static int u16_to_u8(struct value *this, struct value *out)
{
    return value_init_u8(out, this->data.u16);
}

static int u16_to_s16(struct value *this, struct value *out)
{
    return value_init_s16(out, this->data.u16);
}

static int u16_to_u16(struct value *this, struct value *out)
{
    return value_init_u16(out, this->data.u16);
}

static int u16_to_s32(struct value *this, struct value *out)
{
    return value_init_s32(out, this->data.u16);
}

static int u16_to_u32(struct value *this, struct value *out)
{
    return value_init_u32(out, this->data.u16);
}

static int u16_to_s64(struct value *this, struct value *out)
{
    return value_init_s64(out, this->data.u16);
}

static int u16_to_u64(struct value *this, struct value *out)
{
    return value_init_u64(out, this->data.u16);
}

static int u16_to_f32(struct value *this, struct value *out)
{
    return value_init_f32(out, this->data.u16);
}
    
static int u16_to_f64(struct value *this, struct value *out)
{
    return value_init_f64(out, this->data.u16);
}

static int u16_assign(struct value *this, struct value *src)
{
    return value_ops(src)->cast_to_u16(src, this);
}

/*
 * s32 methods.
 */
static int s32_to_s8(struct value *this, struct value *out)
{
    return value_init_s8(out, this->data.s32);
}

static int s32_to_u8(struct value *this, struct value *out)
{
    return value_init_u8(out, this->data.s32);
}

static int s32_to_s16(struct value *this, struct value *out)
{
    return value_init_s16(out, this->data.s32);
}

static int s32_to_u16(struct value *this, struct value *out)
{
    return value_init_u16(out, this->data.s32);
}

static int s32_to_s32(struct value *this, struct value *out)
{
    return value_init_s32(out, this->data.s32);
}

static int s32_to_u32(struct value *this, struct value *out)
{
    return value_init_u32(out, this->data.s32);
}

static int s32_to_s64(struct value *this, struct value *out)
{
    return value_init_s64(out, this->data.s32);
}

static int s32_to_u64(struct value *this, struct value *out)
{
    return value_init_u64(out, this->data.s32);
}

static int s32_to_f32(struct value *this, struct value *out)
{
    return value_init_f32(out, this->data.s32);
}

static int s32_to_f64(struct value *this, struct value *out)
{
    return value_init_f64(out, this->data.s32);
}

static int s32_assign(struct value *this, struct value *src)
{
    return value_ops(src)->cast_to_s32(src, this);
}

static int s32_neg(struct value *op1, struct value *out)
{
    out->type = S32;
    out->data.s32 = -op1->data.s32;
    return 1;
}

static int s32_not(struct value *op1, struct value *out)
{
    out->type = S32;
    out->data.s32 = !op1->data.s32;
    return 1;
}

static int s32_compl(struct value *op1, struct value *out)
{
    out->type = S32;
    out->data.s32 = ~op1->data.s32;
    return 1;
}

static int s32_add(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s32 + op2->data.s32;
    return 1;
}

static int s32_sub(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s32 - op2->data.s32;
    return 1;
}

static int s32_mul(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s32 * op2->data.s32;
    return 1;
}

static int s32_div(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s32 / op2->data.s32;
    return 1;
}

static int s32_mod(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s32 % op2->data.s32;
    return 1;
}

static int s32_and(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s32 & op2->data.s32;
    return 1;
}

static int s32_xor(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s32 ^ op2->data.s32;
    return 1;
}

static int s32_or(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s32 | op2->data.s32;
    return 1;
}

static int s32_shl(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s32 << op2->data.s32;
    return 1;
}

static int s32_shr(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s32 >> op2->data.s32;
    return 1;
}

static int s32_eq(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s32 == op2->data.s32;
    return 1;
}

static int s32_neq(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s32 != op2->data.s32;
    return 1;
}

static int s32_lt(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s32 < op2->data.s32;
    return 1;
}

static int s32_gt(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s32 > op2->data.s32;
    return 1;
}

static int s32_le(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s32 <= op2->data.s32;
    return 1;
}

static int s32_ge(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s32 >= op2->data.s32;
    return 1;
}

/*
 * u32 methods.
 */
static int u32_to_s8(struct value *this, struct value *out)
{
    return value_init_s8(out, this->data.u32);
}

static int u32_to_u8(struct value *this, struct value *out)
{
    return value_init_u8(out, this->data.u32);
}

static int u32_to_s16(struct value *this, struct value *out)
{
    return value_init_s16(out, this->data.u32);
}

static int u32_to_u16(struct value *this, struct value *out)
{
    return value_init_u16(out, this->data.u32);
}

static int u32_to_s32(struct value *this, struct value *out)
{
    return value_init_s32(out, this->data.u32);
}

static int u32_to_u32(struct value *this, struct value *out)
{
    return value_init_u32(out, this->data.u32);
}

static int u32_to_s64(struct value *this, struct value *out)
{
    return value_init_s64(out, this->data.u32);
}

static int u32_to_u64(struct value *this, struct value *out)
{
    return value_init_u64(out, this->data.u32);
}

static int u32_to_f32(struct value *this, struct value *out)
{
    return value_init_f32(out, this->data.u32);
}

static int u32_to_f64(struct value *this, struct value *out)
{
    return value_init_f64(out, this->data.u32);
}

static int u32_assign(struct value *this, struct value *src)
{
    return value_ops(src)->cast_to_u32(src, this);
}

static int u32_neg(struct value *op1, struct value *out)
{
    out->type = U32;
    out->data.u32 = -op1->data.u32;
    return 1;
}

static int u32_not(struct value *op1, struct value *out)
{
    out->type = U32;
    out->data.u32 = !op1->data.u32;
    return 1;
}

static int u32_compl(struct value *op1, struct value *out)
{
    out->type = U32;
    out->data.u32 = ~op1->data.u32;
    return 1;
}

static int u32_add(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U32;
    out->data.u32 = op1->data.u32 + op2->data.u32;
    return 1;
}

static int u32_sub(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U32;
    out->data.u32 = op1->data.u32 - op2->data.u32;
    return 1;
}

static int u32_mul(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U32;
    out->data.u32 = op1->data.u32 * op2->data.u32;
    return 1;
}

static int u32_div(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U32;
    out->data.u32 = op1->data.u32 / op2->data.u32;
    return 1;
}

static int u32_mod(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U32;
    out->data.u32 = op1->data.u32 % op2->data.u32;
    return 1;
}

static int u32_and(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U32;
    out->data.u32 = op1->data.u32 & op2->data.u32;
    return 1;
}

static int u32_xor(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U32;
    out->data.u32 = op1->data.u32 ^ op2->data.u32;
    return 1;
}

static int u32_or(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U32;
    out->data.u32 = op1->data.u32 | op2->data.u32;
    return 1;
}

static int u32_shl(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U32;
    out->data.u32 = op1->data.u32 << op2->data.u32;
    return 1;
}

static int u32_shr(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U32;
    out->data.u32 = op1->data.u32 >> op2->data.u32;
    return 1;
}

static int u32_eq(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.u32 == op2->data.u32;
    return 1;
}

static int u32_neq(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.u32 != op2->data.u32;
    return 1;
}

static int u32_lt(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.u32 < op2->data.u32;
    return 1;
}

static int u32_gt(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.u32 > op2->data.u32;
    return 1;
}

static int u32_le(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.u32 <= op2->data.u32;
    return 1;
}

static int u32_ge(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.u32 >= op2->data.u32;
    return 1;
}

/*
 * s64 methods.
 */
static int s64_to_s8(struct value *this, struct value *out)
{
    return value_init_s8(out, this->data.s64);
}

static int s64_to_u8(struct value *this, struct value *out)
{
    return value_init_u8(out, this->data.s64);
}

static int s64_to_s16(struct value *this, struct value *out)
{
    return value_init_s16(out, this->data.s64);
}

static int s64_to_u16(struct value *this, struct value *out)
{
    return value_init_u16(out, this->data.s64);
}

static int s64_to_s32(struct value *this, struct value *out)
{
    return value_init_s32(out, this->data.s64);
}

static int s64_to_u32(struct value *this, struct value *out)
{
    return value_init_u32(out, this->data.s64);
}

static int s64_to_s64(struct value *this, struct value *out)
{
    return value_init_s64(out, this->data.s64);
}

static int s64_to_u64(struct value *this, struct value *out)
{
    return value_init_u64(out, this->data.s64);
}

static int s64_to_f32(struct value *this, struct value *out)
{
    return value_init_f32(out, this->data.s64);
}

static int s64_to_f64(struct value *this, struct value *out)
{
    return value_init_f64(out, this->data.s64);
}

static int s64_assign(struct value *this, struct value *src)
{
    return value_ops(src)->cast_to_s64(src, this);
}

static int s64_neg(struct value *op1, struct value *out)
{
    out->type = S64;
    out->data.s64 = -op1->data.s64;
    return 1;
}

static int s64_not(struct value *op1, struct value *out)
{
    out->type = S64;
    out->data.s64 = !op1->data.s64;
    return 1;
}

static int s64_compl(struct value *op1, struct value *out)
{
    out->type = S64;
    out->data.s64 = ~op1->data.s64;
    return 1;
}

static int s64_add(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S64;
    out->data.s64 = op1->data.s64 + op2->data.s64;
    return 1;
}

static int s64_sub(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S64;
    out->data.s64 = op1->data.s64 - op2->data.s64;
    return 1;
}

static int s64_mul(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S64;
    out->data.s64 = op1->data.s64 * op2->data.s64;
    return 1;
}

static int s64_div(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S64;
    out->data.s64 = op1->data.s64 / op2->data.s64;
    return 1;
}

static int s64_mod(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S64;
    out->data.s64 = op1->data.s64 % op2->data.s64;
    return 1;
}

static int s64_and(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S64;
    out->data.s64 = op1->data.s64 & op2->data.s64;
    return 1;
}

static int s64_xor(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S64;
    out->data.s64 = op1->data.s64 ^ op2->data.s64;
    return 1;
}

static int s64_or(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S64;
    out->data.s64 = op1->data.s64 | op2->data.s64;
    return 1;
}

static int s64_shl(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S64;
    out->data.s64 = op1->data.s64 << op2->data.s64;
    return 1;
}

static int s64_shr(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S64;
    out->data.s64 = op1->data.s64 >> op2->data.s64;
    return 1;
}

static int s64_eq(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s64 == op2->data.s64;
    return 1;
}

static int s64_neq(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s64 != op2->data.s64;
    return 1;
}

static int s64_lt(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s64 < op2->data.s64;
    return 1;
}

static int s64_gt(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s64 > op2->data.s64;
    return 1;
}

static int s64_le(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s64 <= op2->data.s64;
    return 1;
}

static int s64_ge(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.s64 >= op2->data.s64;
    return 1;
}

/*
 * u64 methods.
 */
static int u64_to_s8(struct value *this, struct value *out)
{
    return value_init_s8(out, this->data.u64);
}

static int u64_to_u8(struct value *this, struct value *out)
{
    return value_init_u8(out, this->data.u64);
}

static int u64_to_s16(struct value *this, struct value *out)
{
    return value_init_s16(out, this->data.u64);
}

static int u64_to_u16(struct value *this, struct value *out)
{
    return value_init_u16(out, this->data.u64);
}

static int u64_to_s32(struct value *this, struct value *out)
{
    return value_init_s32(out, this->data.u64);
}

static int u64_to_u32(struct value *this, struct value *out)
{
    return value_init_u32(out, this->data.u64);
}

static int u64_to_s64(struct value *this, struct value *out)
{
    return value_init_s64(out, this->data.u64);
}

static int u64_to_u64(struct value *this, struct value *out)
{
    return value_init_u64(out, this->data.u64);
}

static int u64_to_f32(struct value *this, struct value *out)
{
    return value_init_f32(out, this->data.u64);
}

static int u64_to_f64(struct value *this, struct value *out)
{
    return value_init_f64(out, this->data.u64);
}

static int u64_assign(struct value *this, struct value *src)
{
    return value_ops(src)->cast_to_u64(src, this);
}

static int u64_neg(struct value *op1, struct value *out)
{
    out->type = U64;
    out->data.u64 = -op1->data.u64;
    return 1;
}

static int u64_not(struct value *op1, struct value *out)
{
    out->type = U64;
    out->data.u64 = !op1->data.u64;
    return 1;
}

static int u64_compl(struct value *op1, struct value *out)
{
    out->type = U64;
    out->data.u64 = ~op1->data.u64;
    return 1;
}

static int u64_add(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U64;
    out->data.u64 = op1->data.u64 + op2->data.u64;
    return 1;
}

static int u64_sub(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U64;
    out->data.u64 = op1->data.u64 - op2->data.u64;
    return 1;
}

static int u64_mul(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U64;
    out->data.u64 = op1->data.u64 * op2->data.u64;
    return 1;
}

static int u64_div(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U64;
    out->data.u64 = op1->data.u64 / op2->data.u64;
    return 1;
}

static int u64_mod(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U64;
    out->data.u64 = op1->data.u64 % op2->data.u64;
    return 1;
}

static int u64_and(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U64;
    out->data.u64 = op1->data.u64 & op2->data.u64;
    return 1;
}

static int u64_xor(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U64;
    out->data.u64 = op1->data.u64 ^ op2->data.u64;
    return 1;
}

static int u64_or(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U64;
    out->data.u64 = op1->data.u64 | op2->data.u64;
    return 1;
}

static int u64_shl(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U64;
    out->data.u64 = op1->data.u64 << op2->data.u64;
    return 1;
}

static int u64_shr(struct value *op1, struct value *op2, struct value *out)
{
    out->type = U64;
    out->data.u64 = op1->data.u64 >> op2->data.u64;
    return 1;
}

static int u64_eq(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.u64 == op2->data.u64;
    return 1;
}

static int u64_neq(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.u64 != op2->data.u64;
    return 1;
}

static int u64_lt(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.u64 < op2->data.u64;
    return 1;
}

static int u64_gt(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.u64 > op2->data.u64;
    return 1;
}

static int u64_le(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.u64 <= op2->data.u64;
    return 1;
}

static int u64_ge(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.u64 >= op2->data.u64;
    return 1;
}

/*
 * f32 methods.
 */
static int f32_to_s8(struct value *this, struct value *out)
{
    return value_init_s8(out, this->data.f32);
}

static int f32_to_u8(struct value *this, struct value *out)
{
    return value_init_u8(out, this->data.f32);
}

static int f32_to_s16(struct value *this, struct value *out)
{
    return value_init_s16(out, this->data.f32);
}

static int f32_to_u16(struct value *this, struct value *out)
{
    return value_init_u16(out, this->data.f32);
}

static int f32_to_s32(struct value *this, struct value *out)
{
    return value_init_s32(out, this->data.f32);
}

static int f32_to_u32(struct value *this, struct value *out)
{
    return value_init_u32(out, this->data.f32);
}

static int f32_to_s64(struct value *this, struct value *out)
{
    return value_init_s64(out, this->data.f32);
}

static int f32_to_u64(struct value *this, struct value *out)
{
    return value_init_u64(out, this->data.f32);
}

static int f32_to_f32(struct value *this, struct value *out)
{
    return value_init_f32(out, this->data.f32);
}

static int f32_to_f64(struct value *this, struct value *out)
{
    return value_init_f64(out, this->data.f32);
}

static int f32_assign(struct value *this, struct value *src)
{
    return value_ops(src)->cast_to_f32(src, this);
}

static int f32_neg(struct value *op1, struct value *out)
{
    value_ops(op1)->cast_to_f64(op1, out);
    return value_ops(out)->neg(out, out);
}

static int f32_not(struct value *op1, struct value *out)
{
    value_ops(op1)->cast_to_f64(op1, out);
    return value_ops(out)->not(out, out);
}

static int f32_add(struct value *op1, struct value *op2, struct value *out)
{
    value_ops(op1)->cast_to_f64(op1, out);
    return value_ops(out)->add(out, op2, out);
}

static int f32_sub(struct value *op1, struct value *op2, struct value *out)
{
    value_ops(op1)->cast_to_f64(op1, out);
    return value_ops(out)->sub(out, op2, out);
}

static int f32_mul(struct value *op1, struct value *op2, struct value *out)
{
    value_ops(op1)->cast_to_f64(op1, out);
    return value_ops(out)->mul(out, op2, out);
}

static int f32_div(struct value *op1, struct value *op2, struct value *out)
{
    value_ops(op1)->cast_to_f64(op1, out);
    return value_ops(out)->div(out, op2, out);
}

static int f32_eq(struct value *op1, struct value *op2, struct value *out)
{
    value_ops(op1)->cast_to_f64(op1, out);
    return value_ops(out)->eq(out, op2, out);
}

static int f32_neq(struct value *op1, struct value *op2, struct value *out)
{
    value_ops(op1)->cast_to_f64(op1, out);
    return value_ops(out)->neq(out, op2, out);
}

static int f32_lt(struct value *op1, struct value *op2, struct value *out)
{
    value_ops(op1)->cast_to_f64(op1, out);
    return value_ops(out)->lt(out, op2, out);
}

static int f32_gt(struct value *op1, struct value *op2, struct value *out)
{
    value_ops(op1)->cast_to_f64(op1, out);
    return value_ops(out)->gt(out, op2, out);
}

static int f32_le(struct value *op1, struct value *op2, struct value *out)
{
    value_ops(op1)->cast_to_f64(op1, out);
    return value_ops(out)->le(out, op2, out);
}

static int f32_ge(struct value *op1, struct value *op2, struct value *out)
{
    value_ops(op1)->cast_to_f64(op1, out);
    return value_ops(out)->ge(out, op2, out);
}

/*
 * f64 methods.
 */
static int f64_to_s8(struct value *this, struct value *out)
{
    return value_init_s8(out, this->data.f64);
}

static int f64_to_u8(struct value *this, struct value *out)
{
    return value_init_u8(out, this->data.f64);
}

static int f64_to_s16(struct value *this, struct value *out)
{
    return value_init_s16(out, this->data.f64);
}

static int f64_to_u16(struct value *this, struct value *out)
{
    return value_init_u16(out, this->data.f64);
}

static int f64_to_s32(struct value *this, struct value *out)
{
    return value_init_s32(out, this->data.f64);
}

static int f64_to_u32(struct value *this, struct value *out)
{
    return value_init_u32(out, this->data.f64);
}

static int f64_to_s64(struct value *this, struct value *out)
{
    return value_init_s64(out, this->data.f64);
}

static int f64_to_u64(struct value *this, struct value *out)
{
    return value_init_u64(out, this->data.f64);
}

static int f64_to_f32(struct value *this, struct value *out)
{
    return value_init_f32(out, this->data.f64);
}

static int f64_to_f64(struct value *this, struct value *out)
{
    return value_init_f64(out, this->data.f64);
}

static int f64_assign(struct value *this, struct value *src)
{
    return value_ops(src)->cast_to_f64(src, this);
}

static int f64_neg(struct value *op1, struct value *out)
{
    out->type = F64;
    out->data.f64 = -op1->data.f64;
    return 1;
}

static int f64_not(struct value *op1, struct value *out)
{
    out->type = S32;
    out->data.s32 = !op1->data.f64;
    return 1;
}

static int f64_add(struct value *op1, struct value *op2, struct value *out)
{
    out->type = F64;
    out->data.f64 = op1->data.f64 + op2->data.f64;
    return 1;
}

static int f64_sub(struct value *op1, struct value *op2, struct value *out)
{
    out->type = F64;
    out->data.f64 = op1->data.f64 - op2->data.f64;
    return 1;
}

static int f64_mul(struct value *op1, struct value *op2, struct value *out)
{
    out->type = F64;
    out->data.f64 = op1->data.f64 * op2->data.f64;
    return 1;
}

static int f64_div(struct value *op1, struct value *op2, struct value *out)
{
    out->type = F64;
    out->data.f64 = op1->data.f64 / op2->data.f64;
    return 1;
}

static int f64_eq(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.f64 == op2->data.f64;
    return 1;
}

static int f64_neq(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.f64 != op2->data.f64;
    return 1;
}

static int f64_lt(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.f64 < op2->data.f64;
    return 1;
}

static int f64_gt(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.f64 > op2->data.f64;
    return 1;
}

static int f64_le(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.f64 <= op2->data.f64;
    return 1;
}

static int f64_ge(struct value *op1, struct value *op2, struct value *out)
{
    out->type = S32;
    out->data.s32 = op1->data.f64 >= op2->data.f64;
    return 1;
}

/*
 * Method tables.
 */
const struct value_operations value_ops[VALUE_TYPES] = {
    {   /* S8 */
        s8_to_s8, s8_to_u8, s8_to_s16, s8_to_u16,
        s8_to_s32, s8_to_u32, s8_to_s64, s8_to_u64,
        s8_to_f32, s8_to_f64,
        s8_assign,

        dummy_neg, dummy_not, dummy_compl,
        dummy_add, dummy_sub, dummy_mul, dummy_div, dummy_mod,
        dummy_and, dummy_xor, dummy_or, dummy_shl, dummy_shr,
        dummy_eq, dummy_neq, dummy_lt, dummy_gt, dummy_le, dummy_ge
    }, 
    {   /* U8 */
        u8_to_s8, u8_to_u8, u8_to_s16, u8_to_u16,
        u8_to_s32, u8_to_u32, u8_to_s64, u8_to_u64,
        u8_to_f32, u8_to_f64,
        u8_assign,

        dummy_neg, dummy_not, dummy_compl,
        dummy_add, dummy_sub, dummy_mul, dummy_div, dummy_mod,
        dummy_and, dummy_xor, dummy_or, dummy_shl, dummy_shr,
        dummy_eq, dummy_neq, dummy_lt, dummy_gt, dummy_le, dummy_ge
    },
    {   /* S16 */
        s16_to_s8, s16_to_u8, s16_to_s16, s16_to_u16,
        s16_to_s32, s16_to_u32, s16_to_s64, s16_to_u64,
        s16_to_f32, s16_to_f64,
        s16_assign,

        dummy_neg, dummy_not, dummy_compl,
        dummy_add, dummy_sub, dummy_mul, dummy_div, dummy_mod,
        dummy_and, dummy_xor, dummy_or, dummy_shl, dummy_shr,
        dummy_eq, dummy_neq, dummy_lt, dummy_gt, dummy_le, dummy_ge
    },
    {   /* U16 */
        u16_to_s8, u16_to_u8, u16_to_s16, u16_to_u16,
        u16_to_s32, u16_to_u32, u16_to_s64, u16_to_u64,
        u16_to_f32, u16_to_f64,
        u16_assign,

        dummy_neg, dummy_not, dummy_compl,
        dummy_add, dummy_sub, dummy_mul, dummy_div, dummy_mod,
        dummy_and, dummy_xor, dummy_or, dummy_shl, dummy_shr,
        dummy_eq, dummy_neq, dummy_lt, dummy_gt, dummy_le, dummy_ge
    },
    {   /* S32 */
        s32_to_s8, s32_to_u8, s32_to_s16, s32_to_u16,
        s32_to_s32, s32_to_u32, s32_to_s64, s32_to_u64,
        s32_to_f32, s32_to_f64,
        s32_assign,

        s32_neg, s32_not, s32_compl,
        s32_add, s32_sub, s32_mul, s32_div, s32_mod,
        s32_and, s32_xor, s32_or, s32_shl, s32_shr,
        s32_eq, s32_neq, s32_lt, s32_gt, s32_le, s32_ge
    },
    {   /* U32 */
        u32_to_s8, u32_to_u8, u32_to_s16, u32_to_u16,
        u32_to_s32, u32_to_u32, u32_to_s64, u32_to_u64,
        u32_to_f32, u32_to_f64,
        u32_assign,

        u32_neg, u32_not, u32_compl,
        u32_add, u32_sub, u32_mul, u32_div, u32_mod,
        u32_and, u32_xor, u32_or, u32_shl, u32_shr,
        u32_eq, u32_neq, u32_lt, u32_gt, u32_le, u32_ge
    },
    {   /* S64 */
        s64_to_s8, s64_to_u8, s64_to_s16, s64_to_u16,
        s64_to_s32, s64_to_u32, s64_to_s64, s64_to_u64,
        s64_to_f32, s64_to_f64,
        s64_assign,

        s64_neg, s64_not, s64_compl,
        s64_add, s64_sub, s64_mul, s64_div, s64_mod,
        s64_and, s64_xor, s64_or, s64_shl, s64_shr,
        s64_eq, s64_neq, s64_lt, s64_gt, s64_le, s64_ge
    },
    {   /* U64 */
        u64_to_s8, u64_to_u8, u64_to_s16, u64_to_u16,
        u64_to_s32, u64_to_u32, u64_to_s64, u64_to_u64,
        u64_to_f32, u64_to_f64,
        u64_assign,

        u64_neg, u64_not, u64_compl,
        u64_add, u64_sub, u64_mul, u64_div, u64_mod,
        u64_and, u64_xor, u64_or, u64_shl, u64_shr,
        u64_eq, u64_neq, u64_lt, u64_gt, u64_le, u64_ge
    },
    {   /* F32 */
        f32_to_s8, f32_to_u8, f32_to_s16, f32_to_u16,
        f32_to_s32, f32_to_u32, f32_to_s64, f32_to_u64,
        f32_to_f32, f32_to_f64,
        f32_assign,

        f32_neg, f32_not, dummy_unop,
        f32_add, f32_sub, f32_mul, f32_div, dummy_mod,
        dummy_nop, dummy_nop, dummy_nop, dummy_nop, dummy_nop,
        f32_eq, f32_neq, f32_lt, f32_gt, f32_le, f32_ge
    },
    {   /* F64 */
        f64_to_s8, f64_to_u8, f64_to_s16, f64_to_u16,
        f64_to_s32, f64_to_u32, f64_to_s64, f64_to_u64,
        f64_to_f32, f64_to_f64,
        f64_assign,

        f64_neg, f64_not, dummy_unop,
        f64_add, f64_sub, f64_mul, f64_div, dummy_mod,
        dummy_nop, dummy_nop, dummy_nop, dummy_nop, dummy_nop,
        f64_eq, f64_neq, f64_lt, f64_gt, f64_le, f64_ge
    }
};
