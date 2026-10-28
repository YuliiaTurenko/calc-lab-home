// Yuliia Turenko 260ADB165
// https://github.com/YulliaTurenko/calc-lab-home>
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: ChatGPT (https://chatgpt.com/share/6ae1af10-5730-83eb-a2c1-32b26e447d9c)

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <dirent.h>

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

    // AI (ChatGPT): prompt "in a C function that appends data 
    // to a dynamically growing buffer, what to write after 
    // if (need > b->cap) to handle the buffer capacity correctly..."
    // Link: https://chatgpt.com/s/t_6ae1993a1a088191abd4958a92411104
    // I used the idea of starting with an initial capacity of 64 bytes
    // and doubling the capacity until it is large enough. 
    // I also used realloc to resize the buffer.
    if (need > b->cap)
    {
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

// AI (ChatGPT): prompt "How should I define Token and Parser structs in C
// for a parser that tokenizes numbers, operators, parentheses, and
// end-of-input markers, while tracking token positions and errors?"
// Link: https://chatgpt.com/s/t_6ae1992918c48191aed50d668a540fad
// I used AI to help design the data structures for storing token
// information, tracking the input position, and reporting parse errors.
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

// AI (ChatGPT): prompt "How do I check whether a directory exists
// in C and create it if it does not? Write a function that handles
// errors and explain the use of stat, S_ISDIR, mkdir, and fprintf."
// Link: https://chatgpt.com/s/t_6ae19c547d7881918c70b180c0d23078
// I used AI to help implement directory checking and creation,
// including error handling with stat and S_ISDIR.
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


// AI (ChatGPT): prompt "Help me implement a descent parser for arithmetic expressions. 
// Write apply_op handling division by zero and non-finite results with correct error positions..."
// Link: https://chatgpt.com/s/t_6ae1a4bcb08c8191b7b28f9555a9a174
// I used AI to implement correctly arithmetic operations in apply_op function and to guide 
// the implementation of the parsing algorithms.
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


// Evaluating the whole text

// Appends one result line. "%lld" for integers and "%.15g" for floats (rule 10).
static int format_value(Buf *out, Value v)
{
    char tmp[64];
    int n;

    if (v.is_float)
        n = snprintf(tmp, sizeof tmp, "%.15g\n", v.d);
    else
        n = snprintf(tmp, sizeof tmp, "%lld\n", v.i);

    if (n < 0)
        return -1;

    return buf_append(out, tmp, (size_t)n);
}

// Evaluates the whole text line by line.
static int eval_text(const char *src, Buf *out, size_t *err_pos)
{
    Parser p;
    p.src = src;
    p.cur = 0;
    p.err_pos = 0;
    p.tok.kind = T_EOL;
    p.tok.pos = 0;
    p.tok.ival = 0;
    p.tok.dval = 0.0;
    *err_pos = 0;

    // line_start is an index into the whole text and never a copy of a line.
    size_t line_start = 0;
    while (src[line_start] != '\0')
    {
        size_t i = line_start;
        while (src[i] == ' ' || src[i] == '\t')
            i++;

        // Blank line or comment line (first non-blank char is '#', rule 2):
        // no output, just jump to the start of the next line.
        if (src[i] == '\n' || src[i] == '\0' || src[i] == '#')
        {
            const char *nl = strchr(src + i, '\n');
            if (nl == NULL)
                break; // last line without '\n'

            line_start = (size_t)(nl - src) + 1;
            continue;
        }

        p.cur = line_start;
        next_token(&p);
        Value v = parse_expr(&p);

        // If something is left on the line, it is an error at that token. This is where an extra ')' is caught.
        if (!p.err_pos && p.tok.kind != T_EOL)
            set_error(&p, p.tok.pos);

        // Rule 3: the first error ends everything, no partial results.
        if (p.err_pos)
        {
            *err_pos = p.err_pos;
            return 0;
        }

        if (format_value(out, v) != 0)
            return -1;

        // The current token is T_EOL: its position is the '\n' (or the end of the text). 
        // The next line starts right after it.
        line_start = p.tok.pos;
        if (src[line_start] == '\n')
            line_start++;
    }

    return 0;
}


// Evaluates ONE file and writes the result file into outdir.
// Returns 0 on success (also when the result is ERROR:<pos>), -1 on I/O problems.
static int process_file(const char *in_path, const char *outdir)
{
    Buf text;
    buf_init(&text);
    if (read_whole_file(in_path, &text) != 0)
    {
        buf_free(&text);
        return -1; // the message is already printed; outdir is not created
    }

    Buf out;
    buf_init(&out);
    size_t err_pos = 0;
    int status = eval_text(text.data, &out, &err_pos);
    buf_free(&text); // the input is not needed any more

    if (status != 0)
    {
        fprintf(stderr, "Out of memory\n");
        buf_free(&out);
        return -1;
    }

    // AI (ChatGPT): prompt "Help me implement the part that handles 'err_pos > 0' using a dynamic buffer...
    // Link: https://chatgpt.com/s/t_6ae1ab1ac8f481919f014bf998afb98a
    // I used AI to understand how to discard partial results and build the required error message in the output buffer.

    // Rule 3: with an error the file contains ONLY "ERROR:<pos>",
    // so the results collected so far are thrown away.
    if (err_pos > 0)
    {
        buf_free(&out);
        buf_init(&out);

        char msg[64];
        int n = snprintf(msg, sizeof msg, "ERROR:%zu\n", err_pos);
        
        if (n < 0 || buf_append(&out, msg, (size_t)n) != 0)
        {
            buf_free(&out);
            return -1;
        }
    }

    char out_path[1024];
    int rc = -1;
    if (ensure_dir(outdir) == 0 &&
        build_output_path(out_path, sizeof out_path, outdir, in_path) == 0 &&
        write_whole_file(out_path, out.data, out.len) == 0)
    {
        rc = 0;
    }

    buf_free(&out);
    return rc;
}

// Handles "-d DIR": every regular *.txt file directly inside DIR.
static int process_dir(const char *dir, const char *outdir)
{
    DIR *d = opendir(dir);
    if (d == NULL)
    {
        fprintf(stderr, "Cannot open folder '%s': %s\n", dir, strerror(errno));
        return -1;
    }

    // Create the output folder even if there are no .txt files inside.
    if (ensure_dir(outdir) != 0)
    {
        closedir(d);
        return -1;
    }

    // AI (ChatGPT): prompt "How do I iterate through a directory in C,
    // find regular .txt files, safely build their paths, and ignore
    // directories or special files? Explain the relevant POSIX functions."
    // Link: https://chatgpt.com/s/t_6ae1acd562e0819190f0b446e5ac2782
    // I used AI to understand directory iteration, filename filtering, path construction, and checking file types.
    int rc = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL)
    {
        const char *name = entry->d_name;
        size_t len = strlen(name);

        if (len <= 4 || strcmp(name + len - 4, ".txt") != 0)
            continue;

        char path[4096];
        int n = snprintf(path, sizeof path, "%s/%s", dir, name);
        
        if (n < 0 || (size_t)n >= sizeof path)
        {
            fprintf(stderr, "Path is too long: %s/%s\n", dir, name);
            rc = -1;
            continue;
        }

        struct stat st;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        if (process_file(path, outdir) != 0)
            rc = -1;
    }

    closedir(d);
    return rc;
}

static void usage(void)
{
    fprintf(stderr, "usage: calc [-d DIR | --dir DIR] [-o OUTDIR | --output-dir OUTDIR] [input.txt]\n");
}


int main(int argc, char **argv)
{
    const char *dir = NULL, *outdir = NULL, *input = NULL;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dir") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Option '%s' needs a value\n", argv[i]);
                usage();
                return 1;
            }
            dir = argv[++i];
        }
        else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output-dir") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Option '%s' needs a value\n", argv[i]);
                usage();
                return 1;
            }
            outdir = argv[++i];
        }
        else if (argv[i][0] == '-')
        {
            fprintf(stderr, "Unknown option '%s'\n", argv[i]);
            usage();
            return 1;
        }
        else
        {
            if (input != NULL)
            {
                fprintf(stderr, "Only one input file is allowed\n");
                usage();
                return 1;
            }
            input = argv[i];
        }
    }

    // We need exactly one: a folder (-d) or an input file.
    if ((dir == NULL) == (input == NULL))
    {
        usage();
        return 1;
    }

    // Default output folder: <input_base>_<name>_<studentid>, where input_base is the file name 
    // without ".txt" or, with -d, the folder's name.
    char default_out[512];
    if (outdir == NULL)
    {
        char base[256];
        base_name(dir != NULL ? dir : input, base, sizeof base);
        snprintf(default_out, sizeof default_out, "%s_%s_%s", base, STUDENT_NAME, STUDENT_ID);
        outdir = default_out;
    }

    int status = (dir != NULL) ? process_dir(dir, outdir) : process_file(input, outdir);

    return status == 0 ? 0 : 1;
}