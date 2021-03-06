#define _DEFAULT_SOURCE /* vsnprintf(3) */
#include "cli.h"
#include "defines.h"
#include "ramfuck.h"

#include "config.h"
#include "eval.h"
#include "hits.h"
#include "lex.h"
#include "line.h"
#include "opt.h"
#include "parse.h"
#include "ptrace.h"
#include "search.h"
#include "symbol.h"
#include "target.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if ADDR_BITS == 64
# define ctx_addr_type(ctx) (((ctx)->addr_size == sizeof(uint64_t)) ? U64 : U32)
#else
# define ctx_addr_type(ctx) (U32)
#endif
extern int ramfuck_read(struct ramfuck *, addr_t, void *, size_t);
extern int ramfuck_write(struct ramfuck *, addr_t, void *, size_t);

static void human_readable_size(size_t bytes, int *size, char *suffix)
{
    static const char *suffixes = "BKMGTPEZ?";
    const char *p = suffixes;
    while (*p != '?' && bytes > 1024) {
        bytes /= 1024;
        p++;
    }
    while (bytes > 1024) bytes /= 1024;
    *size = bytes;
    *suffix = *p;

}

/*
 * Utility functions for parsing the CLI input.
 */
static int eol(const char *p)
{
    return !*p;
}

static void skip_spaces(const char **pin)
{
    while (isspace(**pin)) (*pin)++;
}

static int accept(const char **pin, const char *what)
{
    size_t i;
    for (i = 0; what[i] && what[i] == (*pin)[i]; i++);
    if (!what[i] && (!(*pin)[i] || isspace((*pin)[i]))) {
        *pin += i;
        skip_spaces(pin);
        return 1;
    }
    return 0;
}

static int eat_item(const char **pin, const char **pstart, const char **pend)
{
    int depth = 0;
    int unwrap = *(*pstart = *pin) == '(';
    for (*pend = *pin; **pend && (depth || !isspace(**pend)); (*pend)++) {
        if (unwrap && !depth && *pend != *pin)
            unwrap = 0;
        depth += (**pend == '(') - (**pend == ')');
    }
    if (depth)
        return 0;

    *pin = *pend;
    *pstart += unwrap;
    *pend -= unwrap;
    skip_spaces(pin);
    return 1;
}

static enum value_type accept_type(const char **pin)
{
    enum value_type type;
    const char *l, *r, *in0 = *pin;
    if (eat_item(pin, &l, &r) && (type = value_type_from_substring(l, r-l)))
        return type;
    *pin = in0;
    return 0;
}

static int accept_value(const char **pin, enum value_type value_type,
                        enum value_type addr_type, int positional,
                        struct value *out)
{
    const char *start, *in0;
    struct parser parser;
    parser_init(&parser);
    start = in0 = *pin;
    if (!positional || eat_item(pin, &start, &parser.end)) {
        struct ast *ast;
        parser.quiet = 1;
        parser.addr_type = addr_type;
        if ((ast = parse_expression(&parser, start))) {
            struct ast *cast;
            if (ast->node_type == AST_CAST && (ast->value_type & PTR)) {
                cast = ast;
                ast = ((struct ast_unary *)ast)->child;
                free(cast);
            }
            if (ast->value_type != value_type) {
                if ((cast = ast_cast_new(value_type, ast))) {
                    ast = cast;
                } else {
                    ast_delete(ast);
                    ast = NULL;
                }
            }
            if (ast) {
                int ok = ast_evaluate(ast, out);
                ast_delete(ast);
                if (ok) {
                    if (!positional)
                        *pin = parser.in - (parser.symbol->type == LEX_EOL);
                    return 1;
                }
                errf("cli: evaluating %s value expression failed",
                     value_type_to_string(value_type));
            } else {
                errf("cli: out-of-memory for AST cast node of accepted value");
            }
        } else {
            errf("cli: parsing %s value expression failed",
                 value_type_to_string(value_type));
        }
    }
    *pin = in0;
    return 0;
}

static int accept_addr(const char **pin, enum value_type addr_type,
                       int positional, addr_t *out)
{
    struct value value;
    if (!accept_value(pin, addr_type, addr_type, positional, &value))
        return 0;
#if ADDR_BITS == 64
    *out = (addr_type == U64) ? value.data.u64 : value.data.u32;
#else
    *out = value.data.u32;
#endif
    return 1;
}

static int accept_sint(const char **pin, int positional, intmax_t *out)
{
    struct value value;
    enum value_type value_type, addr_type;
    value_type = SMAX;
    addr_type = ADDR_TYPE;
    if (!accept_value(pin, value_type, addr_type, positional, &value))
        return 0;
    *out = value.data.smax;
    return 1;
}

static size_t fput_value(struct ramfuck *ctx, const struct value *value,
                         int typed, FILE *stream)
{
    size_t len;
    char buf[256], *p;
    size_t (*to_string)(const struct value *, char *, size_t)
        = (ctx->config->cli.base == 16) ? value_to_hexstring : value_to_string;
    if ((len = to_string(value, (p = buf), sizeof(buf))) > sizeof(buf)-1) {
        if (len+1 && (p = malloc(len+1))) {
            to_string(value, p, len+1);
        } else {
            errf("cli: out-of-memory for %" PRIaddru "-byte value string", len);
            p = buf;
        }
    }
    if (typed)
        len += fprintf(stream, "(%s)", value_type_to_string(value->type));
    fputs(p, stream);
    if (p != buf)
        free(p);
    return len;
}

/*
 * Show command result as a 10-base decimal number.
 * Usage: 0d <command>
 */
static int do_0d(struct ramfuck *ctx, const char *in)
{
    int rc;
    unsigned long old;
    if (eol(in)) {
        errf("0d: command expected");
        return 1;
    }
    if (!memcmp(in, "config", 6) && isspace(in[6]))
        return cli_execute_line(ctx, in);
    old = ctx->config->cli.base;
    ctx->config->cli.base = 10;
    rc = cli_execute_line(ctx, in);
    ctx->config->cli.base = old;
    return rc;
}


/*
 * Show command result as a 16-base hexadecimal number.
 * Usage: 0x <command>
 */
static int do_0x(struct ramfuck *ctx, const char *in)
{
    int rc;
    unsigned long old;
    if (eol(in)) {
        errf("0x: command expected");
        return 1;
    }
    if (!memcmp(in, "config", 6) && isspace(in[6]))
        return cli_execute_line(ctx, in);
    old = ctx->config->cli.base;
    ctx->config->cli.base = 16;
    rc = cli_execute_line(ctx, in);
    ctx->config->cli.base = old;
    return rc;
}

/*
 * Attach to a target process specified by PID or URI.
 * Usage: attach <target>
 */
static int do_attach(struct ramfuck *ctx, const char *in)
{
    size_t regions, size;
    struct target *target;
    struct region *mr;
    struct {
        int size;
        char suffix;
    } human;

    if (eol(in)) {
        errf("attach: missing target");
        return 1;
    }

    if (!(target = target_attach(in))) {
        errf("attach: attaching to %s failed", in);
        return 2;
    }

    if (ctx->target) {
        infof("detaching from previous target");
        if (ctx->breaks) {
            if (!ctx->target->run(ctx->target))
                warnf("attach: continuing execution of detach target  failed");
            ctx->breaks = 0;
        }
        target_detach(ctx->target);
    }
    ctx->target = target;

    ctx->breaks = 0;
    ramfuck_break(ctx);
    regions = size = 0;
    target = ctx->target;
    for (mr = target->region_first(target); mr; mr = target->region_next(mr)) {
#if ADDR_BITS == 64
        if ((mr->start + mr->size-1) > UINT32_MAX)
            break;
#endif
        size += mr->size;
        regions++;
    }
    if (mr) {
        do { size += mr->size; regions++; }
        while ((mr = target->region_next(mr)));
        ctx->addr_size = sizeof(uint64_t);
    } else {
        ctx->addr_size = sizeof(uint32_t);
    }
    ramfuck_continue(ctx);

    human_readable_size(size, &human.size, &human.suffix);
    if (regions > 1) {
        infof("attached to target %s (%d%c / %lu memory regions)",
              in, human.size, human.suffix, (unsigned long)regions);
    } else {
        infof("attached to target %s (%d%c)", in, human.size, human.suffix);
    }
    return 0;
}

/*
 * Break (stop) target.
 * Usage: break
 */
static int do_break(struct ramfuck *ctx, const char *in)
{
    if (!eol(in)) {
        errf("break: trailing characters");
        return 1;
    }

    if (!ctx->target) {
        errf("break: not attached to any target");
        return 2;
    }

    if (ctx->breaks) {
        errf("break: target is already stopped");
        return 3;
    }

    if (!ramfuck_break(ctx)) {
        errf("break: stopping failed");
        return 3;
    }

    infof("target stopped");
    return 0;
}

/*
 * Clear hits.
 * Usage: hits
 */
static int do_clear(struct ramfuck *ctx, const char *in)
{
    if (!eol(in)) {
        errf("clear: trailing characters");
        return 1;
    }

    ramfuck_set_hits(ctx, NULL);
    return 0;
}

/*
 * Get or set config.
 * Usage: config
 *        config <item>
 *        config <item> = <value>
 */
static int do_config(struct ramfuck *ctx, const char *in)
{
    return !config_process_line(ctx->config, in);
}

/*
 * Continue target.
 * Usage: continue
 */
static int do_continue(struct ramfuck *ctx, const char *in)
{
    if (!eol(in)) {
        errf("continue: trailing characters");
        return 1;
    }

    if (!ctx->target) {
        errf("continue: not attached to any target");
        return 2;
    }

    if (!ctx->breaks) {
        errf("continue: target is already running");
        return 3;
    }

    if (!ramfuck_continue(ctx)) {
        errf("continue: continuing failed");
        return 4;
    }

    infof("target continued");
    return 0;
}

/*
 * Detach from target.
 * Usage: detach
 */
static int do_detach(struct ramfuck *ctx, const char *in)
{
    if (!eol(in)) {
        errf("detach: trailing characters");
        return 1;
    }
    if (!ctx->target) {
        errf("detach: not attached to any target");
        return 1;
    }

    if (ctx->breaks) {
        if (!ctx->target->run(ctx->target))
            warnf("detach: continuing execution of target");
        ctx->breaks = 0;
    }

    target_detach(ctx->target);
    ctx->target = NULL;
    infof("detached");
    return 0;
}

/*
 * Print hex dump.
 * Usage: hex <addr>
 *        hex <addr> <len>
 */
static int do_hex(struct ramfuck *ctx, const char *in)
{
    enum value_type addr_type;
    addr_t addr;
    addr_t len;
    int size;
    char suffix;
    unsigned char *buf;
    unsigned char *p;

    if (!ctx->target) {
        errf("hex: attach first");
        return 1;
    }

    if (eol(in)) {
        errf("hex: address & length expected");
        return 2;
    }

    addr_type = ctx_addr_type(ctx);
    if (!accept_addr(&in, addr_type, 1, &addr)) {
        errf("hex: invalid address");
        return 3;
    }

    if (eol(in)) {
        len = ctx->config->block.size;
    } else if (!accept_addr(&in, addr_type, 0, &len)) {
        errf("hex: invalid len");
        return 4;
    }

    if (!(buf = malloc(len))) {
        human_readable_size((size_t)len, &size, &suffix);
        errf("hex: error allocating %" PRIaddru " bytes (%d%c) for a buffer",
             len, size, suffix);
        return 5;
    }

    if (!ramfuck_read(ctx, addr, buf, len)) {
        human_readable_size((size_t)len, &size, &suffix);
        errf("hex: error reading %" PRIaddru " bytes (%d%c) from"
             " address 0x%08" PRIaddr, len, size, suffix, addr);
        free(buf);
        return 6;
    }

    p = buf;
    while (len) {
        int i;
        fprintf(stdout, "0x%08" PRIaddr, addr);

        fputs("  ", stdout);
        for (i = 0; i < 16; i++) {
            if (i < len) {
                fprintf(stdout, "%02x", p[i]);
            } else {
                fputs("  ", stdout);
            }
            if ((i & 1) && i < 16-1)
                fputc(' ', stdout);
        }
        fputs("  ", stdout);
        for (i = 0; i < 16 && i < len; i++) {
            fputc(isgraph(p[i]) ? p[i] : '.', stdout);
        }
        fputc('\n', stdout);

        if (len < 16)
            break;
        addr += 16;
        len -= 16;
        p += 16;
    }
    fflush(stdout);
    free(buf);

    return 0;
}

/*
 * Evaluate expression and print its value.
 * Usage: eval <expr>
 */
static int do_eval(struct ramfuck *ctx, const char *in)
{
    struct parser parser;
    struct ast *ast;
    int ok;
    parser_init(&parser);
    parser.quiet = 1;
    parser.addr_type = ctx_addr_type(ctx);
    parser.target = ctx->target;
    if ((ast = parse_expression(&parser, in))) {
        struct value out = {0};
        if (parser.has_deref) ramfuck_break(ctx);
        ok = ast_evaluate(ast, &out);
        if (parser.has_deref) ramfuck_continue(ctx);
        ast_delete(ast);
        if (ok) {
            fput_value(ctx, &out, 0, stdout);
            fputc('\n', stdout);
        }
    }
    return (parser.errors || !ast) ? 1 : (ok ? 0 : 2);
}

/*
 * Explain expression, i.e., print in Reverse Polish Notation (RPN).
 * Usage: explain <expr>
 */
static int do_explain(struct ramfuck *ctx, const char *in)
{
    int rc;
    struct symbol_table *symtab;

    if (eol(in)) {
        errf("explain: expression expected");
        return 1;
    }

    /* Isn't it pretty? Like a Down's child */
    if ((symtab = symbol_table_new(ctx))) {
        struct parser parser;
        struct value value, address;
        struct ast *ast;

        parser_init(&parser);
        value_init_s32(&value, 42);
        symbol_table_add(symtab, "value", S32, &value.data);
#if ADDR_BITS == 64
        if (ctx->addr_size == sizeof(uint64_t)) {
            value_init_u64(&address, UINT64_C(0x123456789ABCDEF));
            parser.addr_type = U64;
        } else {
            address.data.u64 = 0;
            value_init_u32(&address, 0x12345678);
            parser.addr_type = U32;
        }
#else
        value_init_u32(&address, 0x12345678);
        parser.addr_type = U32;
#endif
        symbol_table_add(symtab, "pointer", U16PTR, &address.data);

        parser.symtab = symtab;
        parser.target = ctx->target;
        if ((ast = parse_expression(&parser, in))) {
            struct value out = {0};
            printf("rpn: ");
            ast_print(ast);
            printf("\n");

            if (parser.has_deref)
                ramfuck_break(ctx);
            if (ast_evaluate(ast, &out)) {
                struct ast *ast_opt;
                if ((ast_opt = ast_optimize(ast))) {
                    struct value out_opt;
                    printf("opt: ");
                    ast_print(ast_opt);
                    printf("\n");

                    if (ast_evaluate(ast_opt, &out_opt)) {
                        struct value *l = &out;
                        struct value *r = &out_opt;
                        if (l->type == r->type) {
                            size_t size;
                            if (l->type & PTR)
                                size = value_type_sizeof(parser.addr_type);
                            else size = value_sizeof(l);
                            if (!memcmp(&l->data, &r->data, size)) {
                                fput_value(ctx, &out, 1, stdout);
                                fputc('\n', stdout);
                                rc = 0;
                            } else {
                                errf("explain: optimization gives wrong value");
                                rc = 8;
                            }
                        } else {
                            errf("explain: optimization gives wrong type");
                            rc = 7;
                        }
                    } else {
                        errf("explain: evaluation of optimized AST failed");
                        rc = 6;
                    }
                    ast_delete(ast_opt);
                } else {
                    errf("explain: optimization of AST faild");
                    rc = 5;
                }
            } else {
                errf("explain: evaluation of AST failed");
                rc = 4;
            }
            if (parser.has_deref)
                ramfuck_continue(ctx);
            ast_delete(ast);
        } else {
            errf("explain: %d parse errors", parser.errors);
            rc = 3;
        }
        symbol_table_delete(symtab);
    } else {
        errf("explain: creating a symbol table failed");
        rc = 2;
    }

    return rc;
}

/*
 * Filter current hits.
 * Usage: filter <expression>
 */
static int do_filter(struct ramfuck *ctx, const char *in)
{
    struct hits *hits;

    if (eol(in)) {
        errf("filter: expression expected");
        return 1;
    }

    if (!ctx->hits || !ctx->hits->size) {
        infof("filter: zero hits");
        return 2;
    }

    hits = filter(ctx, ctx->hits, in);
    if (hits == ctx->hits)
        return 3;

    ramfuck_set_hits(ctx, hits);
    return 0;
}

/*
 * List current hits.
 * Usage: list
 */
static int do_list(struct ramfuck *ctx, const char *in)
{
    size_t i;
    struct target *target;

    if (!eol(in)) {
        errf("list: trailing characters");
        return 1;
    }

    if (!ctx->hits || !ctx->hits->size) {
        infof("list: zero hits");
        return 0;
    }

    target = ctx->target;
    ramfuck_break(ctx);
    for (i = 0; i < ctx->hits->size; i++) {
        struct hit *hit = &ctx->hits->items[i];
        if (!ctx->config->cli.quiet) {
            fprintf(stdout, "%lu. *(%s *)",
                    (unsigned long)i+1, value_type_to_string(hit->type));
        } else {
            fprintf(stdout, "%s ", value_type_to_string(hit->type));
        }
        fprintf(stdout, "0x%08" PRIaddr, hit->addr);
        fputs(ctx->config->cli.quiet ? " " : " = ", stdout);
        if (target) {
            size_t size;
            struct value value;
            value.type = hit->type;
            if (hit->type & PTR) {
                value.data.addr = 0;
                size = ctx->addr_size;
            } else {
                size = value_sizeof(&value);
            }
            if (target->read(target, hit->addr, &value.data, size)) {
                fput_value(ctx, &value, 0, stdout);
                fputc('\n', stdout);
                continue;
            }
        }
        fprintf(stdout, "???\n");
    }
    ramfuck_continue(ctx);

    return 0;
}

/*
 * Show memory maps of target.
 * Usage: maps
 */
static int do_maps(struct ramfuck *ctx, const char *in)
{
    char *buf;
    size_t size;
    struct target *target;
    struct region *mr;

    if (!eol(in)) {
        errf("maps: trailing characters");
        return 1;
    }

    if (!ctx->target) {
        errf("maps: attach first");
        return 2;
    }

    if (!(buf = malloc((size = 128)))) {
        errf("maps: cannot allocate line buffer");
        return 3;
    }

    target = ctx->target;
    for (mr = target->region_first(target); mr; mr = target->region_next(mr)) {
        size_t len = region_snprint(mr, buf, size);
        if (len + 1 > size) {
            char *new = realloc(buf, len + 1);
            if (new) {
                buf = new;
                size = len + 1;
                if (region_snprint(mr, buf, size) != len)
                    warnf("maps: inconsistent memory region line formatting");
            } else {
                errf("maps: error reallocating line buffer (will truncate)");
            }
        }
        fprintf(stdout, "%s\n", buf);
    }

    free(buf);
    return 0;
}

/*
 * Peek address.
 * Usage: peek <type> <addr>
 *        peek <hit_index>
 */
static int do_peek(struct ramfuck *ctx, const char *in)
{
    addr_t addr;
    enum value_type type;
    intmax_t index;
    struct value out;
    size_t size;

    if (eol(in)) {
        errf("peek: type & address or index expected");
        return 1;
    }

    type = accept_type(&in);
    if (eol(in)) {
        errf("peek: %s expression expected", type ? "address" : "hit index");
        return 2;
    }

    if (type) {
        if (!accept_addr(&in, ctx_addr_type(ctx), 0, &addr)) {
            errf("peek: evaluating address value failed");
            return 3;
        }
        index = -1;
    } else {
        intmax_t index0;
        if (!accept_sint(&in, 0, &index0)) {
            errf("peek: evaluating hit index failed");
            return 4;
        }
        if (!ctx->hits || !ctx->hits->size) {
            errf("peek: bad index %" PRIdMAX " (0 hits)", index0);
            return 5;
        }
        index = (index0 < 0) ? index0 + ctx->hits->size : index0 - 1;
        if (!(0 <= index && index < ctx->hits->size)) {
            errf("peek: bad index %" PRIdMAX " not in 1..%lu",
                 index0, (unsigned long)ctx->hits->size);
            return 6;
        }
        addr = ctx->hits->items[index].addr;
        type = ctx->hits->items[index].type;
    }

    if (!eol(in)) {
        errf("peek: trailing characters after %s",
            (index == -1) ? "type & address" : "hit index");
        return 7;
    }

    out.type = type;
    size = value_type_sizeof((type & PTR) ? ctx_addr_type(ctx) : type);

    if (index != -1)
        fprintf(stdout, "%" PRIdMAX ". ", index + 1);
    fprintf(stdout, "*(%s *)0x%08" PRIaddr " = ",
            value_type_to_string(type), addr);
    if (ramfuck_read(ctx, addr, &out.data, size)) {
        fput_value(ctx, &out, 0, stdout);
    } else {
        fprintf(stdout, "%s", "???");
    }
    fputc('\n', stdout);
    return 0;
}

/*
 * Poke value.
 * Usage: poke <type> <addr> <value>
 *        poke <hit_index> <value>
 */
static int do_poke(struct ramfuck *ctx, const char *in)
{
    enum value_type type;
    intmax_t index;
    addr_t addr;
    struct symbol_table *symtab;
    struct parser parser;
    struct ast *ast, *cast;
    struct value value, out;
    size_t size;
    int ok;

    if (eol(in)) {
        errf("poke: type & addr & value or index & value expected");
        return 1;
    }

    if (!ctx->target) {
        errf("poke: attach to target first");
        return 2;
    }

    if ((type = accept_type(&in))) {
        if (eol(in)) {
            errf("poke: address expected after type");
            return 3;
        }
        if (!accept_addr(&in, ctx_addr_type(ctx), 1, &addr)) {
            errf("poke: evaluating address value failed");
            return 4;
        }
        index = -1;
    } else {
        intmax_t index0;
        if (!accept_sint(&in, 1, &index0)) {
            errf("poke: evaluating hit index failed");
            return 5;
        }
        if (!ctx->hits || !ctx->hits->size) {
            errf("poke: bad index %" PRIdMAX " (0 hits)", index0);
            return 6;
        }
        index = (index0 < 0) ? index0 + ctx->hits->size : index0 - 1;
        if (!(0 <= index && index < ctx->hits->size)) {
            errf("poke: bad index %" PRIdMAX " not in 1..%lu",
                 index0, (unsigned long)ctx->hits->size);
            return 7;
        }
        addr = ctx->hits->items[index].addr;
        type = ctx->hits->items[index].type;
    }

    if (eol(in)) {
        errf("poke: value expression expected after %s",
             (index == -1) ? "type & address" : "hit index");
        return 8;
    }

    parser_init(&parser);
    parser.addr_type = ctx_addr_type(ctx);
    parser.target = ctx->target;
    size = value_type_sizeof((type & PTR) ? parser.addr_type : type);
    if (strstr(in, "addr") || strstr(in, "value") /* hack ... */) {
        if (!(symtab = symbol_table_new(ctx))) {
            errf("poke: out-of-memory for symbol table");
            return 9;
        }
        if (strstr(in, "addr")) {
            union value_data *data = (union value_data *)&addr;
#if ADDR_BITS == 64
            if (parser.addr_type == U32) {
                addr = (uint32_t)addr;
                data->u32 = (uint32_t)addr;
            }
#endif
            symbol_table_add(symtab, "addr", parser.addr_type, data);
        }
        if (strstr(in, "value")) {
            value.type = type;
            if (ramfuck_read(ctx, addr, &value.data, size)) {
                symbol_table_add(symtab, "value", value.type, &value.data);
            } else {
                errf("poke: error reading %lu bytes from address 0x%08" PRIaddr,
                     (unsigned long)size, addr);
                symbol_table_delete(symtab);
                return 10;
            }
        }
        parser.symtab = symtab;
    } else {
        symtab = NULL;
    }

    if (!(ast = parse_expression(&parser, in))) {
        errf("poke: %d parse errors", parser.errors);
        if (symtab) symbol_table_delete(symtab);
        return 11;
    }
    if (!(cast = ast_cast_new((type & PTR) ? parser.addr_type : type, ast))) {
        errf("poke: out-of-memory for typecast AST");
        if (symtab) symbol_table_delete(symtab);
        ast_delete(ast);
        return 12;
    }
    ast = cast;

    if (parser.has_deref) ramfuck_break(ctx);
    ok = ast_evaluate(ast, &out);
    if (parser.has_deref) ramfuck_continue(ctx);
    if (symtab) symbol_table_delete(symtab);
    ast_delete(ast);
    if (!ok) {
        errf("poke: evaluating value expression failed");
        return 13;
    }

    if (!ramfuck_write(ctx, addr, &out.data, size)) {
        errf("poke: error writing %lu bytes to address 0x%08" PRIaddr,
             (unsigned long)size, addr);
        return 14;
    }

    out.type = type;
    if (index != -1)
        fprintf(stdout, "%" PRIdMAX ". ", index + 1);
    fprintf(stdout, "*(%s *)0x%08" PRIaddr " = ",
            value_type_to_string(type), addr);
    fput_value(ctx, &out, 0, stdout);
    fputc('\n', stdout);
    return 0;
}

/*
 * Quit ramfuck.
 * Usage: quit
 *        q
 *        exit
 */
static int do_quit(struct ramfuck *ctx, const char *in) {
    if (!eol(in)) {
        errf("quit: trailing characters");
        return 1;
    }

    if (ctx->target) {
        if (ctx->breaks) {
            if (!ctx->target->run(ctx->target))
                warnf("quit: continuing execution of target");
            ctx->breaks = 0;
        }
        target_detach(ctx->target);
        ctx->target = NULL;
    }

    ramfuck_quit(ctx);
    return ctx->rc;
}

/*
 * Read memory and write it to file (or stdout if path is -).
 * Usage: read <addr> <len> <path>
 */
static int do_read(struct ramfuck *ctx, const char *in)
{
    enum value_type addr_type;
    addr_t addr, len;
    const char *path;
    FILE *file;
    char *buf, *p;
    int size;
    char suffix;

    if (!ctx->target) {
        errf("read: attach first");
        return 1;
    }

    if (eol(in)) {
        errf("read: address & length & file expected");
        return 2;
    }

    addr_type = ctx_addr_type(ctx);
    if (!accept_addr(&in, addr_type, 1, &addr)) {
        errf("read: invalid address");
        return 3;
    }

    if (eol(in)) {
        errf("read: length expected");
        return 4;
    }

    if (!accept_addr(&in, addr_type, 1, &len)) {
        errf("read: invalid length");
        return 5;
    }
    human_readable_size((size_t)len, &size, &suffix);

    if (!(buf = malloc(len))) {
        errf("read: error allocating %" PRIaddru " bytes (%d%c) for a buffer",
             len, size, suffix);
        return 6;
    }

    if (!ramfuck_read(ctx, addr, buf, len)) {
        errf("read: error reading %" PRIaddru " bytes (%d%c) from"
             " address 0x%08" PRIaddr, len, size, suffix, addr);
        free(buf);
        return 7;
    }

    path = in;
    if (path[0] == '-' && path[1] == '\0') {
        file = stdout;
    } else if (!(file = fopen(path, "wb"))) {
        errf("read: error opening output file %s for writing", path);
        free(buf);
        return 8;
    }

    p = buf;
    while (len > 0 && !feof(file) && !ferror(file)) {
        size_t ret = fwrite(p, sizeof(char), len, file);
        if (ret > 0) {
            len -= ret;
            p += ret;
        }
    }
    fflush(file);
    if (path[0] != '-' || path[1] != '\0')
        fclose(file);
    free(buf);
    if (p == buf) {
        errf("read: error writing to file %s", path);
        return 9;
    }

    if (path[0] != '-' || path[1] != '\0') {
        human_readable_size((size_t)(p - buf), &size, &suffix);
        infof("%" PRIaddru " bytes (%d%c) from address 0x%08" PRIaddr
              " written to %s", (addr_t)(p - buf), size, suffix, addr, path);
    }

    if (len > 0) {
        human_readable_size((size_t)len, &size, &suffix);
        errf("read: error writing last %" PRIaddru " bytes (%d%c) to file %s",
             len, size, suffix, path);
        return 10;
    }

    return 0;
}

/*
 * Undo undo.
 * Usage: redo
 */
static int do_redo(struct ramfuck *ctx, const char *in)
{
    if (!eol(in)) {
        errf("redo: trailing characters");
        return 1;
    }

    if (!ramfuck_redo(ctx)) {
        errf("redo: redo unavailable");
        return 2;
    }

    return 0;
}

/*
 * Initial search.
 * Usage: search <expression>
 *        search <type> <expression>
 * where 'type' is one of: s8, u8, s16, u16, s32, u32, s64, u64, f32, f64.
 */
static int do_search(struct ramfuck *ctx, const char *in)
{
    enum value_type type;
    struct hits *hits;

    if (eol(in)) {
        errf("search: missing type");
        return 1;
    }

    if (!ctx->target) {
        errf("search: attach to target first");
        return 2;
    }

    if (!(type = accept_type(&in)))
        type = S32;

    hits = search(ctx, type, in);
    if (!hits)
        return 3;

    ramfuck_set_hits(ctx, hits);
    return 0;
}

#ifndef NO_FLOAT_VALUES
/*
 * Measure running time of a command.
 * Usage: time <command>
 */
static int do_time(struct ramfuck *ctx, const char *in)
{
    int rc;
    clock_t start, end;

    start = clock();
    rc = cli_execute_line(ctx, in);
    end = clock();

    printf("%gs\n", (double)(end - start) / CLOCKS_PER_SEC);
    return rc;
}
#endif

/*
 * Undo.
 * Usage: undo
 */
static int do_undo(struct ramfuck *ctx, const char *in)
{
    if (!eol(in)) {
        errf("undo: trailing characters");
        return 1;
    }

    if (!ramfuck_undo(ctx)) {
        errf("undo: undo unavailable");
        return 2;
    }

    return 0;
}

/*
 * Write memory from a file (or stdin if path is -) to target memory.
 * Usage: write <addr> <len> <path>
 */
static int do_write(struct ramfuck *ctx, const char *in)
{
    enum value_type addr_type;
    addr_t addr, len, left;
    const char *path;
    FILE *file;
    char *buf, *p;
    int size, ok;
    char suffix;

    if (!ctx->target) {
        errf("write: attach first");
        return 1;
    }

    if (eol(in)) {
        errf("write: address & length & file expected");
        return 2;
    }

    addr_type = ctx_addr_type(ctx);
    if (!accept_addr(&in, addr_type, 1, &addr)) {
        errf("write: invalid address");
        return 3;
    }

    if (eol(in)) {
        errf("write: length expected");
        return 4;
    }

    if (!accept_addr(&in, addr_type, 1, &len)) {
        errf("write: invalid length");
        return 5;
    }
    human_readable_size((size_t)len, &size, &suffix);

    if (!(buf = malloc(len))) {
        errf("write: error allocating %" PRIaddru " bytes (%d%c) for a buffer",
             len, size, suffix);
        return 6;
    }

    path = in;
    if (path[0] == '-' && path[1] == '\0') {
        file = stdin;
    } else if (!(file = fopen(path, "rb"))) {
        errf("write: error opening input file %s for reading", path);
        free(buf);
        return 7;
    }

    p = buf;
    left = len;
    while (left > 0 && !feof(file) && !ferror(file)) {
        size_t ret = fread(p, sizeof(char), left, file);
        if (ret > 0) {
            left -= ret;
            p += ret;
        }
    }
    if (path[0] != '-' || path[1] != '\0')
        fclose(file);
    if (p == buf) {
        errf("write: error reading from input file %s", path);
        free(buf);
        return 8;
    } else if (left > 0) {
        human_readable_size((size_t)left, &size, &suffix);
        errf("write: error reading last %" PRIaddru " bytes (%d%c) of file %s",
             left, size, suffix, path);
        free(buf);
        return 9;
    }

    ok = ramfuck_write(ctx, addr, buf, len);
    free(buf);
    if (!ok) {
        errf("write: error writing %" PRIaddru " bytes (%d%c) from"
             " address 0x%08" PRIaddr, len, size, suffix, addr);
        return 10;
    }

    if (path[0] != '-' || path[1] != '\0') {
        infof("%" PRIaddru " bytes (%d%c) from file %s written to address"
              " 0x%08" PRIaddr, len, size, suffix, path, addr);
    }
    return 0;
}

int cli_execute_line(struct ramfuck *ctx, const char *in)
{
    int rc = 0;
    skip_spaces(&in);
    if (accept(&in, "0d")) {
        rc = do_0d(ctx, in);
    } else if (accept(&in, "0x")) {
        rc = do_0x(ctx, in);
    } else if (accept(&in, "and")) {
        rc = ctx->rc ? ctx->rc : cli_execute_line(ctx, in);
    } else if (accept(&in, "attach")) {
        rc = do_attach(ctx, in);
    } else if (accept(&in, "break")) {
        rc = do_break(ctx, in);
    } else if (accept(&in, "clear")) {
        rc = do_clear(ctx, in);
    } else if (accept(&in, "config")) {
        rc = do_config(ctx, in);
    } else if (accept(&in, "continue")) {
        rc = do_continue(ctx, in);
    } else if (accept(&in, "detach")) {
        rc = do_detach(ctx, in);
    } else if (accept(&in, "hex")) {
        rc = do_hex(ctx, in);
    } else if (accept(&in, "explain")) {
        rc = do_explain(ctx, in);
    } else if (accept(&in, "filter") || accept(&in, "next")) {
        rc = do_filter(ctx, in);
    } else if (accept(&in, "ls") || accept(&in, "list")) {
        rc = do_list(ctx, in);
    } else if (accept(&in, "m") || accept(&in, "maps") || accept(&in, "mem")) {
        rc = do_maps(ctx, in);
    } else if (accept(&in, "or")) {
        rc = ctx->rc ? cli_execute_line(ctx, in) : ctx->rc;
    } else if (accept(&in, "peek")) {
        rc = do_peek(ctx, in);
    } else if (accept(&in, "poke")) {
        rc = do_poke(ctx, in);
    } else if (accept(&in, "quit") || accept(&in, "q") || accept(&in, "exit")) {
        rc = do_quit(ctx, in);
    } else if (accept(&in, "read")) {
        rc = do_read(ctx, in);
    } else if (accept(&in, "redo")) {
        rc = do_redo(ctx, in);
    } else if (accept(&in, "search")) {
        rc = do_search(ctx, in);
#ifndef NO_FLOAT_VALUES
    } else if (accept(&in, "time")) {
        rc = do_time(ctx, in);
#endif
    } else if (accept(&in, "undo")) {
        rc = do_undo(ctx, in);
    } else if (accept(&in, "write")) {
        rc = do_write(ctx, in);
    } else if (!eol(in) && (rc = do_eval(ctx, in)) == 1) {
        size_t i;
        for (i = 0; i < INT_MAX && in[i] && !isspace(in[i]); i++);
        errf("cli: unknown command '%.*s'", (int)i, in);
    }

    return (ctx->rc = rc);
}

int cli_execute(struct ramfuck *ctx, char *in)
{
    int rc = 0;
    char *lstart, *lend;
    for (lstart = in; lstart && ramfuck_running(ctx); lstart = lend + 1) {
        char *comment, *start, *end;
        if ((lend = strchr(lstart, '\n')))
            *lend = '\0';
        if ((comment = strchr(lstart, '#')))
            *comment = '\0';
        for (start = lstart; start && ramfuck_running(ctx); start = end + 1) {
            if ((end = strchr(start, ';')))
                *end = '\0';
            rc = cli_execute_line(ctx, start);
            if (!end) break;
            *end = ';';
        }
        if (comment) *comment = '#';
        if (!lend) break;
        *lend = '\n';
    }
    return rc;
}

int cli_execute_format(struct ramfuck *ctx, const char *format, ...)
{
    va_list args;
    char buf[BUFSIZ];
    size_t len;
    int rc;

    va_start(args, format);
    len = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (len < sizeof(buf)-1) {
        rc = cli_execute(ctx, buf);
    } else {
        int ok;
        char *p;
        if ((p = malloc(len + 1))) {
            va_start(args, format);
            ok = vsnprintf(p, len + 1, format, args) == len;
            va_end(args);
            if (ok) {
                rc = cli_execute(ctx, p);
            } else {
                errf("cli: inconsistent command formatting");
                rc = 2;
            }
            free(p);
        } else {
            errf("cli: out-of-memory for formatted command");
            rc = 1;
        }
    }

    return rc;
}

int cli_main_loop(struct ramfuck *ctx)
{
    while (ramfuck_running(ctx)) {
        char *line = ramfuck_get_line(ctx);
        if (line) {
            cli_execute(ctx, line);
            ramfuck_free_line(ctx, line);
        } else {
            ramfuck_quit(ctx);
        }
    }
    return ctx->rc;
}
