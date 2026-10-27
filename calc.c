// Yuliia Turenko 260ADB165
// https://github.com/YulliaTurenko/calc-lab-home>
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: ChatGPT

#include <stdio.h>

// Growable buffer
// Used for (a) the whole input file and (b) the whole output text.
// The output must be collected in memory first, because if ANY error appears
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
        size_t new_cap = b->cap ? b->cap : 64; //AI
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



int main()
{
    
    return 0;
}