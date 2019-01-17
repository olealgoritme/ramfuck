#define _DEFAULT_SOURCE
#include "parse.h"
#include "ramfuck.h"
#include "lex.h"
#include "value.h"

#include <memory.h>
#include <stdarg.h>
#include <stdlib.h>

struct parser {
    const char *in;
    struct symbol_table *symtab;
    int quiet;
    int errors;

    struct lex_token *symbol;   /* Symbol being processed. */
    struct lex_token *accepted; /* Last accepted symbol. */
    struct lex_token tokens[2]; /* symbol and accepted fields point here. */
};

static struct ast *expression(struct parser *p);
static struct ast *conditional_expression(struct parser *p);
static struct ast *or_expression(struct parser *p);
static struct ast *xor_expression(struct parser *p);
static struct ast *and_expression(struct parser *p);
static struct ast *equality_expression(struct parser *p);
static struct ast *relational_expression(struct parser *p);
static struct ast *shift_expression(struct parser *p);
static struct ast *addsub_expression(struct parser *p);
static struct ast *muldiv_expression(struct parser *p);
static struct ast *cast_expression(struct parser *p);
static struct ast *unary_expression(struct parser *p);
static struct ast *factor(struct parser *p);

static void parse_error(struct parser *p, const char *format, ...)
{
    if (!p->quiet) {
        va_list args;
        va_start(args, format);
        fputs("parse: ", stderr);
        vfprintf(stderr, format, args);
        fputc('\n', stderr);
        va_end(args);
    }
    p->errors++;
}

static int next_symbol(struct parser *p)
{
    if (!lexer(&p->in, p->symbol)) {
        parse_error(p, "lexer error");
        do { lexer(&p->in, p->symbol); } while (p->symbol->type != LEX_EOL);
        return 0;
    }
    return 1;
}

static int accept(struct parser *p, enum lex_token_type sym)
{
    if (p->symbol->type != sym)
        return 0;

    /* Swap p->accepted and p->symbol */
    p->accepted = p->symbol;
    if (p->symbol == &p->tokens[0]) {
        p->symbol = &p->tokens[1];
    } else { /* p->symbol == &p->tokens[1] */
        p->symbol = &p->tokens[0];
    }

    /* Next symbol */
    return next_symbol(p);
}

static int expect(struct parser *p, enum lex_token_type sym)
{
    if (!accept(p, sym)) {
        char symstr[64];
        lex_token_to_string(p->symbol, symstr, sizeof(symstr));
        parse_error(p, "unexpected symbol '%s'", symstr);
        return 0;
    }
    return 1;
}

int parse_expression(const char *in, struct symbol_table *symtab,
                     int quiet, struct ast **pout)
{
    struct ast *out;
    struct parser p;
    if (pout == NULL)
        pout = &out;

    memset(&p, 0, sizeof(struct parser));
    p.in = in;
    p.symbol = &p.tokens[0];
    p.accepted = &p.tokens[1];
    p.symtab = symtab;
    p.quiet = quiet;
    next_symbol(&p);
    *pout = expression(&p);

    if (p.symbol->type != LEX_EOL) {
        parse_error(&p, "EOL expected");
        do { next_symbol(&p); } while (p.symbol->type != LEX_EOL);
    } else if (*pout == NULL) {
        parse_error(&p, "empty input");
    }

    if (p.errors > 0 && *pout)
        ast_delete(*pout);

    return p.errors;
}

/*
 * Productions. See doc/grammar.txt
 */
static struct ast *expression(struct parser *p)
{
    return (p->symbol->type == LEX_EOL) ? NULL : conditional_expression(p);
}

static struct ast *conditional_expression(struct parser *p)
{
    struct ast *root;
    root = or_expression(p);

    while (root && (accept(p, LEX_AND_COND) || accept(p, LEX_OR_COND))) {
        enum ast_type type;
        struct ast *left, *right;

        type = lex_to_ast_type(p->accepted->type);
        left = root;
        right = or_expression(p);

        if (right) {
            root = ast_binop_new(type, left, right);
            root->value_type = SINT;
        } else {
            ast_delete(root);
            root = NULL;
        }
    }

    return root;
}

static struct ast *or_expression(struct parser *p)
{
    struct ast *root;
    root = xor_expression(p);

    while (root && accept(p, LEX_OR)) {
        struct ast *left, *right;

        left = root;
        right = xor_expression(p);
        root = NULL;

        if (!right) {
            ast_delete(left);
            break;
        }

        if ((left->value_type & INT) && (right->value_type & INT)) {
            root = ast_binop_new(AST_OR, left, right);
            root->value_type = HIGHER_TYPE(left->value_type, right->value_type);
        } else {
            parse_error(p, "invalid operands for '|'");
            ast_delete(left);
            ast_delete(right);
        }
    }

    return root;
}
static struct ast *xor_expression(struct parser *p)
{
    struct ast *root;
    root = and_expression(p);

    while (root && accept(p, LEX_XOR)) {
        struct ast *left, *right;

        left = root;
        right = and_expression(p);
        root = NULL;

        if (!right) {
            ast_delete(left);
            break;
        }

        if ((left->value_type & INT) && (right->value_type & INT)) {
            root = ast_binop_new(AST_XOR, left, right);
            root->value_type = HIGHER_TYPE(left->value_type, right->value_type);
        } else {
            parse_error(p, "invalid operands for '^'");
            ast_delete(left);
            ast_delete(right);
        }
    }

    return root;
}
static struct ast *and_expression(struct parser *p)
{
    struct ast *root;
    root = equality_expression(p);

    while (root && accept(p, LEX_AND)) {
        struct ast *left, *right;

        left = root;
        right = equality_expression(p);
        root = NULL;

        if (!right) {
            ast_delete(left);
            break;
        }

        if ((left->value_type & INT) && (right->value_type & INT)) {
            root = ast_binop_new(AST_AND, left, right);
            root->value_type = HIGHER_TYPE(left->value_type, right->value_type);
        } else {
            parse_error(p, "invalid operands for '&'");
            ast_delete(left);
            ast_delete(right);
        }
    }

    return root;
}

static struct ast *equality_expression(struct parser *p)
{
    struct ast *root;
    root = relational_expression(p);

    if (root && (accept(p, LEX_EQ) || accept(p, LEX_NEQ))) {
        enum ast_type type;
        struct ast *left, *right;

        type = lex_to_ast_type(p->accepted->type);
        left = root;
        right = relational_expression(p);
        root = NULL;

        if (!right) {
            ast_delete(left);
            return NULL;
        }

        if ((left->value_type & (INT|FPU))
                && (right->value_type & (INT|FPU))) {
            root = ast_binop_new(type, left, right);
            root->value_type = SINT;
        } else {
            parse_error(p, "invalid operands for equality operator");
            ast_delete(left);
            ast_delete(right);
        }
    }

    return root;
}

static struct ast *relational_expression(struct parser *p)
{
    struct ast *root;
    root = shift_expression(p);

    if (root && (accept(p, LEX_LT) || accept(p, LEX_GT)
            || accept(p, LEX_LE) || accept(p, LEX_GE))) {
        enum ast_type type;
        struct ast *left, *right;

        type = lex_to_ast_type(p->accepted->type);
        left = root;
        right = shift_expression(p);
        root = NULL;

        if (!right) {
            ast_delete(left);
            return NULL;
        }

        if ((left->value_type & (INT|FPU))
                && (right->value_type & (INT|FPU))) {
            root = ast_binop_new(type, left, right);
            root->value_type = SINT;
        } else {
            parse_error(p, "invalid operands for relative operator");
            ast_delete(left);
            ast_delete(right);
        }
    }

    return root;
}

static struct ast *shift_expression(struct parser *p)
{
    struct ast *root;
    root = addsub_expression(p);

    while (root && (accept(p, LEX_SHL) || accept(p, LEX_SHR))) {
        enum ast_type type;
        struct ast *left, *right;

        type = lex_to_ast_type(p->accepted->type);
        left = root;
        right = addsub_expression(p);
        root = NULL;

        if (!right) {
            ast_delete(left);
            break;
        }

        if ((left->value_type & INT) && (right->value_type & INT)) {
            root = ast_binop_new(type, left, right);
            root->value_type = left->value_type;
        } else {
            parse_error(p, "invalid operand types for binary shift");
            ast_delete(left);
            ast_delete(right);
        }
    }

    return root;
}

static struct ast *addsub_expression(struct parser *p)
{
    struct ast *root; 
    root = muldiv_expression(p);

    while (root && (accept(p, LEX_ADD) || accept(p, LEX_SUB))) {
        enum ast_type type;
        struct ast *left, *right;

        type = lex_to_ast_type(p->accepted->type);
        left = root;
        right = muldiv_expression(p);
        root = NULL;

        if (!right) {
            ast_delete(left);
            break;
        }

        if ((left->value_type & (INT|FPU))
                && (right->value_type & (INT|FPU))) {
            root = ast_binop_new(type, left, right);
            root->value_type = HIGHER_TYPE(left->value_type, right->value_type);
        } else {
            parse_error(p, "invalid operands for '+' or '-'");
            ast_delete(left);
            ast_delete(right);
        }
    }

    return root;
}

static struct ast *muldiv_expression(struct parser *p)
{
    struct ast *root = cast_expression(p);

    while (root && (accept(p, LEX_MUL)
                 || accept(p, LEX_DIV)
                 || accept(p, LEX_MOD))) {
        enum ast_type type;
        struct ast *left, *right;
        enum value_type valid_types;

        type = lex_to_ast_type(p->accepted->type);
        left = root;
        right = cast_expression(p);
        root = NULL;

        if (!right) {
            ast_delete(left);
            break;
        }

        valid_types = (type == AST_MOD) ? INT : (INT|FPU);
        if ((left->value_type & valid_types)
                && (right->value_type & valid_types)) {
            root = ast_binop_new(type, left, right);
            root->value_type = HIGHER_TYPE(left->value_type, right->value_type);
        } else {
            parse_error(p, "invalid operands for muldiv operator");
            ast_delete(left);
            ast_delete(right);
        }
    }

    return root;
}

static struct ast *cast_expression(struct parser *p)
{
    if (p->symbol->type == LEX_LEFT_PARENTHESE) {
        struct lex_token peek[2];
        const char *pin = p->in;
        if (lexer(&pin, &peek[0]) && lexer(&pin, &peek[1])
                && peek[0].type == LEX_IDENTIFIER
                && peek[1].type == LEX_RIGHT_PARENTHESE) {
            char buf[16];
            enum value_type type;
            size_t len = peek[0].value.identifier.len;
            const char *name = peek[0].value.identifier.name;
            if (len < sizeof(buf)-1) {
                buf[len] = '\0';
                type = value_type_from_string(memcpy(buf, name, len));
                if (type && accept(p, LEX_LEFT_PARENTHESE)
                         && accept(p, LEX_IDENTIFIER)
                         && accept(p, LEX_RIGHT_PARENTHESE)) {
                    struct ast *child = cast_expression(p);
                    return child ? ast_cast_new(type, child) : NULL;
                }
            }
        }
    }
    return unary_expression(p);
}

static struct ast *unary_expression(struct parser *p)
{
    struct ast *root;

    if (accept(p, LEX_ADD) || accept(p, LEX_SUB)
            || accept(p, LEX_NOT) || accept(p, LEX_COMPL)) {
        struct ast *child;
        enum ast_type type;

        root = NULL;
        type = lex_to_ast_type(p->accepted->type);
        if (!(child = cast_expression(p))) 
            return NULL;

        if (type == AST_ADD || type == AST_SUB) {
            /* AST_ADD -> AST_UADD, AST_SUB -> LEX_USUB */
            type += AST_UADD - AST_ADD; 

            if (child->value_type & (INT|FPU)) {
                root = ast_unop_new(type, child);
                root->value_type = child->value_type;
            }
        } else { /* AST_NOT or AST_COMPL */
            if (child->value_type & INT) {
                root = ast_unop_new(type, child);
                root->value_type = child->value_type;
            }
        }

        if (!root) {
            parse_error(p, "invalid operands for unary operator");
            ast_delete(child);
        }
    } else {
        root = factor(p);
    }

    return root;
}

static struct ast *factor(struct parser *p)
{
    struct ast *root;

    if (accept(p, LEX_IDENTIFIER)) {
        struct symbol *sym;
        const char *name = p->accepted->value.identifier.name;
        size_t len = p->accepted->value.identifier.len;
        if (p->symtab && (sym = symbol_table_nlookup(p->symtab, name, len))) {
            root = ast_var_new(sym->name, &sym->value);
            root->value_type = sym->value.type;
        } else {
            parse_error(p, "unknown identifier '%.*s'", (int)len, name);
            root = NULL;
        }
    } else if (accept(p, LEX_INTEGER)) {
        root = ast_int_new(p->accepted->value.integer);
        root->value_type = SINT;
    } else if (accept(p, LEX_UINTEGER)) {
        root = ast_uint_new((uintmax_t)p->accepted->value.integer);
        root->value_type = UINT;
    } else if (accept(p, LEX_FLOATING_POINT)) {
        root = ast_float_new(p->accepted->value.fp);
        root->value_type = DOUBLE;
    } else if (accept(p, LEX_LEFT_PARENTHESE)) {
        root = expression(p);
        expect(p, LEX_RIGHT_PARENTHESE);
    } else {
        if (p->symbol->type != LEX_EOL) {
            char symstr[64];
            lex_token_to_string(p->symbol, symstr, sizeof(symstr));
            parse_error(p, "expected a factor but got '%s'", symstr);
        } else {
            parse_error(p, "expected a factor");
        }
        root = NULL;
    }

    return root;
}
