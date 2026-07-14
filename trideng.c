#include <stdio.h>
#include <string.h>

struct reverse_entry {
    const char *token;
    char letter;
};

static const struct reverse_entry reverse_map[] = {
    {"li", 'a'}, {"no", 'b'}, {"kr", 'c'}, {"ku", 'd'}, {"in", 'e'},
    {"il", 'f'}, {"al", 'g'}, {"en", 'h'}, {"de", 'i'}, {"so", 'j'},
    {"ni", 'k'}, {"te", 'l'}, {"ma", 'm'}, {"ak", 'n'}, {"ta", 'o'},
    {"ha", 'p'}, {"pi", 'q'}, {"si", 'r'}, {"zi", 's'}, {"pu", 't'},
    {"lo", 'u'}, {"sk", 'v'}, {"ra", 'w'}, {"gu", 'x'}, {"mi", 'y'},
    {"tu", 'z'}
};

static void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s TextToTranslate\n", program_name);
}

static int is_passthrough(char c) {
    return c == '.' || c == ',' || c == '-' || c == '!' || c == '?' ||
           c == '(' || c == ')';
}

static char decode_token(const char token[3]) {
    size_t count = sizeof(reverse_map) / sizeof(reverse_map[0]);

    for (size_t i = 0; i < count; i++) {
        if (strcmp(token, reverse_map[i].token) == 0) {
            return reverse_map[i].letter;
        }
    }

    return '?';
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    for (int arg = 1; arg < argc; arg++) {
        if (arg > 1) {
            putchar(' ');
        }

        const char *text = argv[arg];
        for (size_t i = 0; text[i] != '\0';) {
            if (text[i] == ' ' || is_passthrough(text[i])) {
                putchar(text[i]);
                i++;
                continue;
            }

            if (text[i + 1] == '\0') {
                putchar('?');
                i++;
                continue;
            }

            char token[3] = {text[i], text[i + 1], '\0'};
            putchar(decode_token(token));
            i += 2;
        }
    }

    putchar('\n');
    return 0;
}
