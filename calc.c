// Yuliia Turenko 260ADB165
// https://github.com/YulliaTurenko/calc-lab-home>
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: ChatGPT

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define STUDENT_NAME "Yuliia"
#define STUDENT_LASTNAME "Turenko"
#define STUDENT_ID "260ADB165"

// Growable buffer
// Used for (a) the whole input file and (b) the whole output text.
// The output must be collected in memory first, because if any error appears
// in the file, the output file must contain only "ERROR:<pos>" (rule 3).

typedef struct
{
    char *data;
    size_t len;
    size_t cap;
} Buf;

static void buf_init(Buf *b)
{
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static int buf_append(Buf *b, const char *s, size_t n)
{
    size_t need = b->len + n + 1;

    if (need > b->cap)
    {
        // AI (ChatGPT): prompt ""
        size_t new_cap = b->cap ? b->cap : 64;
        while (new_cap < need)
            new_cap *= 2;

        char *ptr = realloc(b->data, new_cap);
        if (ptr == NULL)
        {
            printf("Failed to resize memory");
            return -1;
        }

        b->data = ptr;
        b->cap = new_cap;
    }

    if (n > 0)
        memcpy(b->data + b->len, s, n);

    b->len += n;
    b->data[b->len] = '\0';

    return 0;
}

static void buf_free(Buf *b)
{
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

// Tokenizer
// The parser works directly on the file text, the 0-based index into 
// the text + 1 is exactly the required 1-based position, and it automatically counts
// every '\n' and every character of comment lines (rule 4).

typedef enum
{
    T_INT,
    T_FLOAT,
    T_PLUS,
    T_MINUS,
    T_STAR,
    T_SLASH,
    T_POW,
    T_LPAREN,
    T_RPAREN,
    T_EOL,
    T_BAD
} TokKind;

// AI (ChatGPT): prompt ""
typedef struct
{
    TokKind kind;
    size_t pos; // 0-based index of the token's first character
    long long ival; // valid for T_INT
    double dval; // valid for T_FLOAT
} Token;

typedef struct
{
    const char *src; // whole input text, NUL-terminated
    size_t cur; // index of the first character not yet tokenized
    Token tok; // the token the parser is currently "looking at"
    size_t err_pos; // 1-based position of the first error; 0 = no error yet
} Parser;

static void set_error(Parser *p, size_t pos0)
{
    // pos0 is 0-based, we store 1-based.
    if (p->err_pos == 0)
        p->err_pos = pos0 + 1;
}

static void next_token(Parser *p)
{
    const char *s = p->src;
    size_t i = p->cur;

    while (s[i] == ' ' || s[i] == '\t') i++;

    Token token;
    token.pos = i;
    token.ival = 0;
    token.dval = 0.0;

    char c = s[i];
    if (c == '\0' || c == '\n')
    {
        token.kind = T_EOL;
    }
    else if (isdigit((unsigned char)c))
    {
        unsigned long n = 0;

        while (isdigit((unsigned char)s[i]))
        {
            n = n * 10 + (unsigned long)(s[i] - '0');
            i++;
        }

        // A '.' makes a float only when a digit follows it. Otherwise the
        // number ends here and the '.' becomes T_BAD on the NEXT call.
        if (s[i] == '.' && isdigit((unsigned char)s[i + 1]))
        {
            i++;
            while (isdigit((unsigned char)s[i])) i++;

            token.dval = strtod(s + token.pos, NULL);
            token.kind = T_FLOAT;
        }
        else
        {
            token.ival = (long)n;
            token.kind = T_INT;
        }
    }
    else if (c == '+')
    {
        token.kind = T_PLUS;
        i++;
    }
    else if (c == '-')
    {
        token.kind = T_MINUS;
        i++;
    }
    else if (c == '/')
    {
        token.kind = T_SLASH;
        i++;
    }
    else if (c == '(')
    {
        token.kind = T_LPAREN;
        i++;
    }
    else if (c == ')')
    {
        token.kind = T_RPAREN;
        i++;
    }
    else if (c == '*')
    {
        if (s[i + 1] == '*')
        {
            token.kind = T_POW;
            i += 2;
        }
        else
        {
            token.kind = T_STAR;
            i++;
        }
    }
    else
    {
        token.kind = T_BAD;
    }

    p->tok = token;
    p->cur = i;
}


// Files and paths

static int read_whole_file(const char *path, Buf *out)
{
    FILE *f = fopen(path, "rb");

    if (f == NULL)
    {
        fprintf(stderr, "Cannot open '%s': %s\n", path, strerror(errno));
        return -1;
    }

    // Big chunks + a buffer that doubles in buf_append: a file of several
    // megabytes is read in a few calls and never copied again and again.
    char chunk[65536];
    size_t n;
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0)
    {
        if (buf_append(out, chunk, n) != 0)
        {
            fclose(f);
            return -1;
        }
    }

    int failed = ferror(f);
    fclose(f);

    if (failed)
    {
        fprintf(stderr, "Error while reading '%s'\n", path);
        return -1;
    }

    // An empty file never called buf_append with data. Make sure out->data
    // exists and is terminated with '\0', because the parser relies on it.
    if (out->data == NULL && buf_append(out, "", 0) != 0)
        return -1;

    return 0;
}

static int write_whole_file(const char *path, const char *data, size_t len)
{
    FILE *f = fopen(path, "wb");

    if (f == NULL)
    {
        fprintf(stderr, "Cannot write '%s': %s\n", path, strerror(errno));
        return -1;
    }

    if (len > 0 && fwrite(data, 1, len, f) != len)
    {
        fprintf(stderr, "Error while writing '%s'\n", path);
        fclose(f);
        return -1;
    }

    if (fclose(f) != 0)
    {
        fprintf(stderr, "Error while closing '%s'\n", path);
        return -1;
    }

    return 0;
}

// AI (ChatGPT): prompt ""
static int ensure_dir(const char *path)
{
    struct stat st;

    if (stat(path, &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
            return 0;

        fprintf(stderr, "'%s' exists and is not a folder\n", path);

        return -1;
    }

    if (mkdir(path, 0777) != 0)
    {
        fprintf(stderr, "Cannot create folder '%s': %s\n", path, strerror(errno));
        return -1;
    }

    return 0;
}

static void base_name(const char *path, char *dst, size_t dst_size)
{
    if (dst_size == 0)
        return;

    size_t end = strlen(path);
    while (end > 0 && path[end - 1] == '/') // ignore trailing slashes
        end--;

    size_t start = end;
    while (start > 0 && path[start - 1] != '/') // go back to the last '/'
        start--;

    size_t len = end - start;
    if (len >= 4 && strncmp(path + start + len - 4, ".txt", 4) == 0)
        len -= 4; // cut ".txt"

    if (len >= dst_size) // never write past the destination array
        len = dst_size - 1;

    memcpy(dst, path + start, len);
    dst[len] = '\0';
}

static int build_output_path(char *dst, size_t size, const char *outdir, const char *in_path)
{
    char base[256];
    base_name(in_path, base, sizeof base);

    int n = snprintf(dst, size, "%s/%s_%s_%s_%s.txt", 
                    outdir, base, STUDENT_NAME, STUDENT_LASTNAME, STUDENT_ID);
    if (n < 0 || (size_t)n >= size)
    {
        fprintf(stderr, "Output path is too long\n");
        return -1;
    }
    return 0;
}

// Values (integer or float)
// If either operand is a float -> do the operation in double, otherwise in long long. So every result carries its own type.

typedef struct
{
    int is_float;
    long long i;
    double d;
} Value;

static Value make_int(long long v)
{
    Value r;
    r.is_float = 0;
    r.i = v;
    r.d = 0.0;
    return r;
}

static Value make_float(double v)
{
    Value r;
    r.is_float = 1;
    r.i = 0;
    r.d = v;
    return r;
}

static double as_double(Value v)
{
    if (v.is_float){
        return v.d;
    }

    return (double)v.i;
}

static long long int_power(long long base, long long exp)
{
    long long result = 1;
    
    while (exp > 0) {
        result *= base;
        exp--;
    }

    return result;
}


// Recursive descent parser
//  expr := term  { ('+' | '-') term }
//  term := power { ('*' | '/') power }
//  power := primary [ '**' power ]
//  primary := NUMBER | '(' expr ')'
//
// Why precedence is right: expr only adds/subtracts whole terms, and every
// term is already computed by parse_term, so '*' and '/' always happen before
// '+' and '-'. The same idea puts '**' above '*' and '/'.
// Why '**' is right-associative: parse_power calls itself for the exponent,
// so 2 ** 3 ** 2 is read as 2 ** (3 ** 2). The while loops in expr/term
// calculate step by step from the left, so + - * / are left-associative.
//
// Every caller checks p->err_pos after each sub-call, so nothing more is
// calculated after the first error.

static Value parse_expr(Parser *p);

// Does one + - * / step. Here is where int or float arithmetic is chosen:
// if BOTH operands are integers we stay in long long, if at least one is a
// float, both go to double.
static Value apply_op(Parser *p, char op, Value l, Value r, size_t op_pos, size_t rhs_pos)
{
    // Division by zero is reported at the START OF THE RIGHT OPERAND (rhs_pos),
    // for both int and float (1.5 / 0.0 is also an error).
    if (op == '/')
    {
        int zero = r.is_float ? (r.d == 0.0) : (r.i == 0);
        if (zero)
        {
            set_error(p, rhs_pos);
            return make_int(0);
        }
    }

    if (!l.is_float && !r.is_float)
    {
        if (op == '+') return make_int(l.i + r.i);
        if (op == '-') return make_int(l.i - r.i);
        if (op == '*') return make_int(l.i * r.i);

        return make_int(l.i / r.i); // C truncates toward zero, as the task wants
    }

    double a = as_double(l);
    double b = as_double(r);
    double res;

    if (op == '+') res = a + b;
    else if (op == '-') res = a - b;
    else if (op == '*') res = a * b;
    else res = a / b;

    // Infinity or NaN is reported at the OPERATOR (op_pos).
    if (!isfinite(res))
    {
        set_error(p, op_pos);
        return make_int(0);
    }
    return make_float(res);
}

static Value parse_primary(Parser *p)
{
    Token t = p->tok;

    if (t.kind == T_INT)
    {
        next_token(p);
        return make_int(t.ival);
    }
    if (t.kind == T_FLOAT)
    {
        next_token(p);
        return make_float(t.dval);
    }
    if (t.kind == T_LPAREN)
    {
        next_token(p);
        Value v = parse_expr(p); // the whole expression inside the brackets
        if (p->err_pos) return v;

        // A missing ')' is reported at the token we see instead of it.
        // At the end of a line that token is T_EOL, i.e. the '\n'.
        if (p->tok.kind != T_RPAREN)
        {
            set_error(p, p->tok.pos);
            return make_int(0);
        }
        next_token(p);

        return v;
    }

    // Here we need a number or '(' but got something else. This one line
    // covers: invalid character, two operators in a row, missing operand at
    // the end of a line, "()", and unary minus like "-5".
    // The error is at the token the parser was looking at.
    set_error(p, t.pos);

    return make_int(0);
}

static Value parse_power(Parser *p)
{
    Value base = parse_primary(p);
    if (p->err_pos) return base;

    if (p->tok.kind == T_POW)
    {
        size_t op_pos = p->tok.pos;
        next_token(p);
        size_t exp_pos = p->tok.pos; // needed for the negative exponent error
        Value exp = parse_power(p);  // recursion = right-associative
        
        if (p->err_pos) return exp;

        if (!base.is_float && !exp.is_float)
        {
            // int ** int: a negative exponent is an error at the exponent start
            if (exp.i < 0)
            {
                set_error(p, exp_pos);
                return make_int(0);
            }

            return make_int(int_power(base.i, exp.i));
        }

        // At least one float: use pow() in double. 0.5 ** (0 - 1) is fine (= 2),
        // but 10.0 ** 400 or (0 - 8) ** 0.5 give infinity/NaN -> error at the operator.
        double res = pow(as_double(base), as_double(exp));
        if (!isfinite(res))
        {
            set_error(p, op_pos);
            return make_int(0);
        }

        return make_float(res);
    }

    return base;
}

static Value parse_term(Parser *p)
{
    Value left = parse_power(p);
    if (p->err_pos) return left;

    while (p->tok.kind == T_STAR || p->tok.kind == T_SLASH)
    {
        char op = (p->tok.kind == T_STAR) ? '*' : '/';
        size_t op_pos = p->tok.pos;
        next_token(p);
        size_t rhs_pos = p->tok.pos; // saved before parsing the right operand
        Value right = parse_power(p);
        if (p->err_pos) return right;

        left = apply_op(p, op, left, right, op_pos, rhs_pos);
        if (p->err_pos) return left;
    }
    return left;
}

static Value parse_expr(Parser *p)
{
    Value left = parse_term(p);
    if (p->err_pos) return left;

    while (p->tok.kind == T_PLUS || p->tok.kind == T_MINUS)
    {
        char op = (p->tok.kind == T_PLUS) ? '+' : '-';
        size_t op_pos = p->tok.pos;
        next_token(p);
        size_t rhs_pos = p->tok.pos;
        Value right = parse_term(p);
        if (p->err_pos) return right;

        left = apply_op(p, op, left, right, op_pos, rhs_pos);
        if (p->err_pos) return left;
    }

    return left;
}



// TEMPORARY TEST CODE

typedef struct
{
    const char *input;
    const char *expected;
} TestCase;

// Runs the parser on one line
static void eval_one(const char *text, char *result, size_t size)
{
    Parser p;
    p.src = text;
    p.cur = 0;
    p.err_pos = 0;

    next_token(&p);
    Value v = parse_expr(&p);
    if (!p.err_pos && p.tok.kind != T_EOL)
        set_error(&p, p.tok.pos);

    if (p.err_pos)
        snprintf(result, size, "ERROR:%zu", p.err_pos);
    else if (v.is_float)
        snprintf(result, size, "%.15g", v.d);
    else
        snprintf(result, size, "%lld", v.i);
}

static void run_parser_tests(void)
{
    static const TestCase tests[] = {
        {"2 + 5 - 3", "4"},
        {"3 + 2 * 5 - 4", "9"},
        {"5 + 4 / 0 * 2", "ERROR:9"},
        {"7 / 2", "3"},
        {"(0 - 7) / 2", "-3"},
        {"((2 + 3) * (4 + (6 - 1))) - 10", "35"},
        {"2 * ()", "ERROR:6"},
        {"(2 + 3", "ERROR:7"},
        {"(1 + 2))", "ERROR:8"},
        {"2 * 3 ** 2 + 1", "19"},
        {"2 ** 3 ** 2", "512"},
        {"2 ** (1 - 2)", "ERROR:6"},
        {"2 + & 5", "ERROR:5"},
        {"7 + * 3", "ERROR:5"},
        {"10 + - 3", "ERROR:6"},
        {"3 * ", "ERROR:5"},
        {"-5", "ERROR:1"},
        {"3.5 + 2.5 * 2 - 1.0", "7.5"},
        {"0.1 + 0.2", "0.3"},
        {"7 / 2.0", "3.5"},
        {"0.5 ** (0 - 1)", "2"},
        {"10.0 ** 400", "ERROR:6"},
        {"(0 - 8) ** 0.5", "ERROR:9"},
        {"5.", "ERROR:2"},
        {"1.5e3", "ERROR:4"},
    };
    size_t count = sizeof tests / sizeof tests[0];
    int failed = 0;

    for (size_t k = 0; k < count; k++)
    {
        char got[64];
        eval_one(tests[k].input, got, sizeof got);
        if (strcmp(got, tests[k].expected) != 0)
        {
            printf("FAIL: \"%s\" expected %s, got %s\n",
                   tests[k].input, tests[k].expected, got);
            failed++;
        }
    }
    printf("parser: %zu tests, %d failed\n", count, failed);
}

// Reads a file and writes an exact copy to test_out/<base>_<name>_<lastname>_<id>.txt
static void test_files(const char *in_path)
{
    Buf text;
    buf_init(&text);

    if (read_whole_file(in_path, &text) == 0)
    {
        printf("read %zu bytes from %s\n", text.len, in_path);
        char out_path[512];
        if (ensure_dir("test_out") == 0 &&
            build_output_path(out_path, sizeof out_path, "test_out", in_path) == 0 &&
            write_whole_file(out_path, text.data, text.len) == 0)
        {
            printf("wrote a copy to %s\n", out_path);
        }
    }
    buf_free(&text);
}

int main(int argc, char **argv)
{
    run_parser_tests();
    if (argc >= 2)
        test_files(argv[1]);
    return 0;
}