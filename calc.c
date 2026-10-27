// Yuliia Turenko 260ADB165
// https://github.com/YulliaTurenko/calc-lab-home>
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: ChatGPT

#include <stdio.h>

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

        int *ptr = realloc(b->data, new_cap);
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
    char *s = p->src;
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


int main()
{
    
    return 0;
}