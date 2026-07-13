#include <ctype.h>
#include <stdio.h>

static const char *letter_map[26] = {
    "li", "no", "kr", "ku", "in", "il", "al", "en", "de", "so", "ni", "te",
    "ma", "ak", "ta", "ha", "pi", "si", "zi", "pu", "lo", "sk", "ra", "gu",
    "mi", "tu"
};

static void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s TextToTranslate\n", program_name);
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

        const unsigned char *text = (const unsigned char *)argv[arg];
        for (size_t i = 0; text[i] != '\0'; i++) {
            unsigned char lowered = (unsigned char)tolower(text[i]);

            if (lowered >= 'a' && lowered <= 'z') {
                fputs(letter_map[lowered - 'a'], stdout);
            } else {
                putchar(text[i]);
            }
        }
    }

    putchar('\n');
    return 0;
}
