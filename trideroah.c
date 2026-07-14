#include <ctype.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct {
    double x;
    double y;
} CGPoint;

typedef struct {
    double width;
    double height;
} CGSize;

typedef struct {
    CGPoint origin;
    CGSize size;
} CGRect;

enum {
    NO_VALUE = 0,
    YES_VALUE = 1
};

enum {
    NSWindowStyleMaskTitled = 1 << 0,
    NSWindowStyleMaskClosable = 1 << 1,
    NSWindowStyleMaskMiniaturizable = 1 << 2,
    NSWindowStyleMaskResizable = 1 << 3,
    NSBackingStoreBuffered = 2,
    NSApplicationActivationPolicyRegular = 0,
    NSImageScaleProportionallyUpOrDown = 3,
    NSTextAlignmentCenter = 2,
    NSFocusRingTypeDefault = 0
};

typedef struct {
    const char *letter;
    const char *token;
} LetterMap;

typedef struct {
    const char *english;
    const char *token;
} WordMap;

typedef enum {
    PRACTICE_NONE = 0,
    PRACTICE_LETTERS = 1,
    PRACTICE_WORDS = 2
} PracticeMode;

static const char *fallback_asset_root =
    "/Users/tristan/Desktop/skill building/technical/coding/Tristan/Apps/Translation/Translation/Assets.xcassets";
static char asset_root[PATH_MAX];

static const LetterMap letter_map[] = {
    {"a", "li"}, {"b", "no"}, {"c", "kr"}, {"d", "ku"}, {"e", "in"},
    {"f", "il"}, {"g", "al"}, {"h", "en"}, {"i", "de"}, {"j", "so"},
    {"k", "ni"}, {"l", "te"}, {"m", "ma"}, {"n", "ak"}, {"o", "ta"},
    {"p", "ha"}, {"q", "pi"}, {"r", "si"}, {"s", "zi"}, {"t", "pu"},
    {"u", "lo"}, {"v", "sk"}, {"w", "ra"}, {"x", "gu"}, {"y", "mi"},
    {"z", "tu"}
};

static const WordMap word_map[] = {
    {"hello", "eninteteta"},
    {"world", "ratasiteku"},
    {"learn", "teintelisak"},
    {"practice", "hasilikrpudekrin"},
    {"letters", "teinputpuinsizi"},
    {"words", "ratasiukzi"},
    {"glyph", "altenmihaen"},
    {"trideroah", "pusidekuinsitalien"}
};

static id window_ref;
static id scroll_ref;
static id practice_image;
static id practice_prompt;
static id practice_answer;
static id practice_result;
static int current_index = 0;
static int current_word_index = 0;
static PracticeMode current_mode = PRACTICE_NONE;
static double question_started_at = 0.0;
static int letters_practiced = 0;
static int words_practiced = 0;
static double letter_times[64];
static double word_times[64];
static int letter_time_count = 0;
static int word_time_count = 0;
static int awaiting_answer = 0;

static CGRect rect(double x, double y, double width, double height) {
    CGRect value = {{x, y}, {width, height}};
    return value;
}

static SEL selector(const char *name) {
    return sel_registerName(name);
}

static id cls(const char *name) {
    return (id)objc_getClass(name);
}

static id msg_id(id receiver, const char *name) {
    return ((id (*)(id, SEL))objc_msgSend)(receiver, selector(name));
}

static void msg_void(id receiver, const char *name) {
    ((void (*)(id, SEL))objc_msgSend)(receiver, selector(name));
}

static void msg_void_id(id receiver, const char *name, id arg) {
    ((void (*)(id, SEL, id))objc_msgSend)(receiver, selector(name), arg);
}

static void msg_void_bool(id receiver, const char *name, BOOL arg) {
    ((void (*)(id, SEL, BOOL))objc_msgSend)(receiver, selector(name), arg);
}

static void msg_void_int(id receiver, const char *name, int arg) {
    ((void (*)(id, SEL, int))objc_msgSend)(receiver, selector(name), arg);
}

static void msg_void_double(id receiver, const char *name, double arg) {
    ((void (*)(id, SEL, double))objc_msgSend)(receiver, selector(name), arg);
}

static void msg_void_ptr(id receiver, const char *name, const void *arg) {
    ((void (*)(id, SEL, const void *))objc_msgSend)(receiver, selector(name), arg);
}

static id ns_string(const char *text) {
    return ((id (*)(id, SEL, const char *))objc_msgSend)(
        cls("NSString"), selector("stringWithUTF8String:"), text
    );
}

static const char *utf8_string(id string) {
    return ((const char *(*)(id, SEL))objc_msgSend)(string, selector("UTF8String"));
}

static id alloc_init_frame(const char *class_name, CGRect frame) {
    id object = msg_id(cls(class_name), "alloc");
    return ((id (*)(id, SEL, CGRect))objc_msgSend)(
        object, selector("initWithFrame:"), frame
    );
}

static id font(double size) {
    return ((id (*)(id, SEL, double))objc_msgSend)(
        cls("NSFont"), selector("systemFontOfSize:"), size
    );
}

static id color(const char *name) {
    return msg_id(cls("NSColor"), name);
}

static const void *cg_color(const char *name) {
    return ((const void *(*)(id, SEL))objc_msgSend)(color(name), selector("CGColor"));
}

static void add_subview(id parent, id child) {
    msg_void_id(parent, "addSubview:", child);
}

static void set_string(id field, const char *text) {
    msg_void_id(field, "setStringValue:", ns_string(text));
}

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static double clamp_double(double value, double min_value, double max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static void store_time(double *times, int *count, double value) {
    if (*count < 64) {
        times[*count] = value;
        *count += 1;
        return;
    }

    for (int i = 1; i < 64; i++) {
        times[i - 1] = times[i];
    }
    times[63] = value;
}

static int directory_exists(const char *path) {
    return access(path, R_OK | X_OK) == 0;
}

static void use_asset_root_if_found(const char *path, int *found) {
    if (!*found && path && directory_exists(path)) {
        snprintf(asset_root, sizeof(asset_root), "%s", path);
        *found = 1;
    }
}

static void resolve_asset_root(const char *program_path) {
    int found = 0;
    const char *env_assets = getenv("TRIDEROAH_ASSETS");
    use_asset_root_if_found(env_assets, &found);
    use_asset_root_if_found("Assets.xcassets", &found);
    use_asset_root_if_found("assets/Assets.xcassets", &found);

    if (!found && program_path) {
        const char *slash = strrchr(program_path, '/');
        if (slash) {
            char program_dir[PATH_MAX];
            size_t dir_len = (size_t)(slash - program_path);
            if (dir_len >= sizeof(program_dir)) {
                dir_len = sizeof(program_dir) - 1;
            }
            memcpy(program_dir, program_path, dir_len);
            program_dir[dir_len] = '\0';

            char candidate[PATH_MAX];
            snprintf(candidate, sizeof(candidate), "%s/Assets.xcassets", program_dir);
            use_asset_root_if_found(candidate, &found);
            snprintf(candidate, sizeof(candidate), "%s/assets/Assets.xcassets", program_dir);
            use_asset_root_if_found(candidate, &found);
        }
    }

    if (!found) {
        snprintf(asset_root, sizeof(asset_root), "%s", fallback_asset_root);
    }
}

static void image_path(const char *letter, char *buffer, size_t size) {
    snprintf(buffer, size, "%s/%s.imageset/%s.heic", asset_root, letter, letter);
}

static id image_for_letter(const char *letter) {
    char path[1024];
    image_path(letter, path, sizeof(path));
    id image = msg_id(cls("NSImage"), "alloc");
    return ((id (*)(id, SEL, id))objc_msgSend)(
        image, selector("initWithContentsOfFile:"), ns_string(path)
    );
}

static id make_label(const char *text, double x, double y, double width, double height, double size) {
    id field = alloc_init_frame("NSTextField", rect(x, y, width, height));
    set_string(field, text);
    msg_void_bool(field, "setBezeled:", NO_VALUE);
    msg_void_bool(field, "setDrawsBackground:", NO_VALUE);
    msg_void_bool(field, "setEditable:", NO_VALUE);
    msg_void_bool(field, "setSelectable:", NO_VALUE);
    msg_void_id(field, "setFont:", font(size));
    return field;
}

static id make_button(const char *title, id target, SEL action, double x, double y) {
    id button = alloc_init_frame("NSButton", rect(x, y, 96, 32));
    msg_void_id(button, "setTitle:", ns_string(title));
    msg_void_id(button, "setTarget:", target);
    ((void (*)(id, SEL, SEL))objc_msgSend)(button, selector("setAction:"), action);
    return button;
}

static id make_button_sized(const char *title, id target, SEL action, double x, double y, double width) {
    id button = alloc_init_frame("NSButton", rect(x, y, width, 34));
    msg_void_id(button, "setTitle:", ns_string(title));
    msg_void_id(button, "setTarget:", target);
    ((void (*)(id, SEL, SEL))objc_msgSend)(button, selector("setAction:"), action);
    return button;
}

static void style_card(id view) {
    msg_void_bool(view, "setWantsLayer:", YES_VALUE);
    id layer = msg_id(view, "layer");
    msg_void_ptr(layer, "setBackgroundColor:", cg_color("controlBackgroundColor"));
    msg_void_ptr(layer, "setBorderColor:", cg_color("separatorColor"));
    msg_void_double(layer, "setBorderWidth:", 1.0);
    msg_void_double(layer, "setCornerRadius:", 8.0);
}

static void render_home(id self);
static void render_letters(id self);
static void render_stats(id self);
static void next_practice(id self, SEL _cmd, id sender);
static void check_practice(id self, SEL _cmd, id sender);

static id make_page(double height) {
    return alloc_init_frame("FlippedTrideroahView", rect(0, 0, 960, height));
}

static void show_page(id page) {
    msg_void_id(scroll_ref, "setDocumentView:", page);
}

static void add_tabs(id root, id self, int active_tab) {
    const char *titles[] = {"Home", "Letters", "Stats"};
    const char *actions[] = {"showHome:", "showLetters:", "showStats:"};

    for (int i = 0; i < 3; i++) {
        id button = make_button_sized(titles[i], self, selector(actions[i]), 24 + i * 112, 100, 100);
        if (i == active_tab) {
            msg_void_bool(button, "setEnabled:", NO_VALUE);
        }
        add_subview(root, button);
    }
}

static void add_header(id root, id self, int active_tab) {
    id title = make_label("Learn Trideroah", 24, 20, 500, 38, 30);
    add_subview(root, title);

    id subtitle = make_label(
        "Practice the glyphs, tokens, English letters, and words.",
        24,
        62,
        680,
        24,
        14
    );
    msg_void_id(subtitle, "setTextColor:", color("secondaryLabelColor"));
    add_subview(root, subtitle);
    add_tabs(root, self, active_tab);
}

static id make_alphabet_tile(const char *letter, const char *token, double x, double y) {
    id box = alloc_init_frame("FlippedTrideroahView", rect(x, y, 140, 120));
    style_card(box);

    id image_view = alloc_init_frame("NSImageView", rect(24, 12, 92, 70));
    msg_void_id(image_view, "setImage:", image_for_letter(letter));
    msg_void_int(image_view, "setImageScaling:", NSImageScaleProportionallyUpOrDown);
    add_subview(box, image_view);

    char text[32];
    snprintf(text, sizeof(text), "%c  %s", (char)toupper((unsigned char)letter[0]), token);
    id title = make_label(text, 10, 86, 120, 24, 15);
    msg_void_int(title, "setAlignment:", NSTextAlignmentCenter);
    add_subview(box, title);

    return box;
}

static void make_practice_panel(id root, id self, double y) {
    id practice_box = alloc_init_frame("FlippedTrideroahView", rect(24, y, 920, 176));
    style_card(practice_box);
    add_subview(root, practice_box);

    if (current_mode == PRACTICE_LETTERS) {
        practice_image = alloc_init_frame("NSImageView", rect(20, 28, 110, 110));
        msg_void_int(practice_image, "setImageScaling:", NSImageScaleProportionallyUpOrDown);
        add_subview(practice_box, practice_image);
    }

    practice_prompt = make_label("", 160, 28, 650, 30, 18);
    add_subview(practice_box, practice_prompt);

    practice_answer = alloc_init_frame("NSTextField", rect(160, 70, 260, 30));
    if (current_mode == PRACTICE_WORDS) {
        msg_void_id(practice_answer, "setPlaceholderString:", ns_string("Type the English word"));
    } else {
        msg_void_id(practice_answer, "setPlaceholderString:", ns_string("Type the English letter"));
    }
    msg_void_id(practice_answer, "setFont:", font(16));
    msg_void_bool(practice_answer, "setEditable:", YES_VALUE);
    msg_void_bool(practice_answer, "setSelectable:", YES_VALUE);
    msg_void_bool(practice_answer, "setEnabled:", YES_VALUE);
    msg_void_bool(practice_answer, "setBezeled:", YES_VALUE);
    msg_void_int(practice_answer, "setFocusRingType:", NSFocusRingTypeDefault);
    msg_void_id(practice_answer, "setTarget:", self);
    ((void (*)(id, SEL, SEL))objc_msgSend)(
        practice_answer,
        selector("setAction:"),
        selector("checkPractice:")
    );
    add_subview(practice_box, practice_answer);

    id check_button = make_button("Check", self, selector("checkPractice:"), 160, 114);
    id next_button = make_button("Next", self, selector("nextPractice:"), 266, 114);
    add_subview(practice_box, check_button);
    add_subview(practice_box, next_button);

    practice_result = make_label("", 440, 75, 360, 24, 14);
    msg_void_id(practice_result, "setTextColor:", color("secondaryLabelColor"));
    add_subview(practice_box, practice_result);
}

static void ask_new_question(void) {
    char prompt[256];
    question_started_at = now_seconds();
    awaiting_answer = 1;

    if (current_mode == PRACTICE_WORDS) {
        current_word_index = rand() % (int)(sizeof(word_map) / sizeof(word_map[0]));
        snprintf(
            prompt,
            sizeof(prompt),
            "What English word makes '%s'?",
            word_map[current_word_index].token
        );
    } else {
        current_index = rand() % (int)(sizeof(letter_map) / sizeof(letter_map[0]));
        if (practice_image) {
            msg_void_id(practice_image, "setImage:", image_for_letter(letter_map[current_index].letter));
        }
        snprintf(
            prompt,
            sizeof(prompt),
            "Which English letter makes '%s'?",
            letter_map[current_index].token
        );
    }

    set_string(practice_prompt, prompt);
    set_string(practice_answer, "");
    set_string(practice_result, "");
    msg_void_id(window_ref, "makeFirstResponder:", practice_answer);
}

static void next_practice(id self, SEL _cmd, id sender) {
    (void)self;
    (void)_cmd;
    (void)sender;
    ask_new_question();
}

static void check_practice(id self, SEL _cmd, id sender) {
    (void)self;
    (void)_cmd;
    (void)sender;

    id answer_string = msg_id(practice_answer, "stringValue");
    const char *answer = utf8_string(answer_string);
    while (*answer != '\0' && isspace((unsigned char)*answer)) {
        answer++;
    }

    char message[192];
    int correct = 0;

    if (current_mode == PRACTICE_WORDS) {
        char normalized[128];
        size_t answer_len = strlen(answer);
        if (answer_len >= sizeof(normalized)) {
            answer_len = sizeof(normalized) - 1;
        }
        for (size_t i = 0; i < answer_len; i++) {
            normalized[i] = (char)tolower((unsigned char)answer[i]);
        }
        normalized[answer_len] = '\0';
        correct = strcmp(normalized, word_map[current_word_index].english) == 0;
        snprintf(
            message,
            sizeof(message),
            "%s: %s = %s",
            correct ? "Correct" : "Answer",
            word_map[current_word_index].english,
            word_map[current_word_index].token
        );
    } else {
        char expected = letter_map[current_index].letter[0];
        char actual = (char)tolower((unsigned char)answer[0]);
        correct = actual == expected;
        snprintf(
            message,
            sizeof(message),
            "%s: %c = %s",
            correct ? "Correct" : "Answer",
            (char)toupper((unsigned char)expected),
            letter_map[current_index].token
        );
    }

    if (awaiting_answer) {
        double reaction = now_seconds() - question_started_at;
        if (current_mode == PRACTICE_WORDS) {
            words_practiced++;
            store_time(word_times, &word_time_count, reaction);
        } else {
            letters_practiced++;
            store_time(letter_times, &letter_time_count, reaction);
        }
        awaiting_answer = 0;
    }

    msg_void_id(practice_result, "setTextColor:", color(correct ? "systemGreenColor" : "systemRedColor"));
    set_string(practice_result, message);
}

static void add_alphabet_grid(id root, double start_y) {
    id alphabet_heading = make_label("Alphabet", 24, start_y, 200, 28, 20);
    add_subview(root, alphabet_heading);

    for (int i = 0; i < 26; i++) {
        int column = i % 6;
        int row = i / 6;
        double x = 24 + column * 152;
        double y = start_y + 42 + row * 132;
        add_subview(root, make_alphabet_tile(letter_map[i].letter, letter_map[i].token, x, y));
    }
}

static void render_home(id self) {
    id root = make_page(760);
    add_header(root, self, 0);

    id heading = make_label("Home", 24, 154, 300, 28, 20);
    add_subview(root, heading);

    id letter_card = alloc_init_frame("FlippedTrideroahView", rect(24, 198, 430, 120));
    style_card(letter_card);
    add_subview(root, letter_card);
    add_subview(letter_card, make_label("Practice letters", 18, 16, 240, 26, 18));
    id letter_desc = make_label("See a glyph and token, then type the matching English letter.", 18, 48, 360, 44, 14);
    msg_void_id(letter_desc, "setTextColor:", color("secondaryLabelColor"));
    add_subview(letter_card, letter_desc);
    add_subview(letter_card, make_button_sized("Practice letters", self, selector("startLetters:"), 260, 70, 140));

    id word_card = alloc_init_frame("FlippedTrideroahView", rect(490, 198, 430, 120));
    style_card(word_card);
    add_subview(root, word_card);
    add_subview(word_card, make_label("Practice words", 18, 16, 240, 26, 18));
    id word_desc = make_label("Read a Trideroah word token and type the English word.", 18, 48, 340, 44, 14);
    msg_void_id(word_desc, "setTextColor:", color("secondaryLabelColor"));
    add_subview(word_card, word_desc);
    add_subview(word_card, make_button_sized("Practice words", self, selector("startWords:"), 270, 70, 130));

    if (current_mode != PRACTICE_NONE) {
        id practice_heading = make_label(
            current_mode == PRACTICE_WORDS ? "Practice Words" : "Practice Letters",
            24,
            364,
            300,
            28,
            20
        );
        add_subview(root, practice_heading);
        make_practice_panel(root, self, 408);
        ask_new_question();
    }

    show_page(root);
}

static void render_letters(id self) {
    id root = make_page(1040);
    add_header(root, self, 1);
    add_alphabet_grid(root, 154);
    show_page(root);
}

static id make_bar(double x, double y, double width, const char *color_name) {
    id bar = alloc_init_frame("FlippedTrideroahView", rect(x, y, width, 18));
    msg_void_bool(bar, "setWantsLayer:", YES_VALUE);
    id layer = msg_id(bar, "layer");
    msg_void_ptr(layer, "setBackgroundColor:", cg_color(color_name));
    msg_void_double(layer, "setCornerRadius:", 4.0);
    return bar;
}

static void add_graph(id root, const char *title, double *times, int count, double y, const char *bar_color) {
    add_subview(root, make_label(title, 24, y, 300, 26, 18));
    if (count == 0) {
        id empty = make_label("No answers yet.", 24, y + 36, 300, 24, 14);
        msg_void_id(empty, "setTextColor:", color("secondaryLabelColor"));
        add_subview(root, empty);
        return;
    }

    int start = count > 12 ? count - 12 : 0;
    double max_time = 1.0;
    for (int i = start; i < count; i++) {
        if (times[i] > max_time) {
            max_time = times[i];
        }
    }

    for (int i = start; i < count; i++) {
        double row_y = y + 38 + (i - start) * 28;
        char label_text[64];
        snprintf(label_text, sizeof(label_text), "%.2fs", times[i]);
        add_subview(root, make_label(label_text, 24, row_y - 2, 70, 22, 13));
        double width = clamp_double((times[i] / max_time) * 420.0, 8.0, 420.0);
        add_subview(root, make_bar(96, row_y, width, bar_color));
    }
}

static void render_stats(id self) {
    id root = make_page(1040);
    add_header(root, self, 2);

    add_subview(root, make_label("Stats", 24, 154, 300, 28, 20));

    char letter_text[128];
    char word_text[128];
    snprintf(letter_text, sizeof(letter_text), "Letters practiced: %d", letters_practiced);
    snprintf(word_text, sizeof(word_text), "Words practiced: %d", words_practiced);

    id letter_count = make_label(letter_text, 24, 202, 260, 26, 18);
    id word_count = make_label(word_text, 310, 202, 260, 26, 18);
    add_subview(root, letter_count);
    add_subview(root, word_count);

    add_graph(root, "Letter reaction time", letter_times, letter_time_count, 264, "systemBlueColor");
    add_graph(root, "Word reaction time", word_times, word_time_count, 626, "systemGreenColor");
    show_page(root);
}

static void show_home(id self, SEL _cmd, id sender) {
    (void)_cmd;
    (void)sender;
    render_home(self);
}

static void show_letters(id self, SEL _cmd, id sender) {
    (void)_cmd;
    (void)sender;
    render_letters(self);
}

static void show_stats(id self, SEL _cmd, id sender) {
    (void)_cmd;
    (void)sender;
    render_stats(self);
}

static void start_letters(id self, SEL _cmd, id sender) {
    (void)_cmd;
    (void)sender;
    current_mode = PRACTICE_LETTERS;
    render_home(self);
}

static void start_words(id self, SEL _cmd, id sender) {
    (void)_cmd;
    (void)sender;
    current_mode = PRACTICE_WORDS;
    render_home(self);
}

static void build_window(id self) {
    id app = msg_id(cls("NSApplication"), "sharedApplication");

    window_ref = msg_id(cls("NSWindow"), "alloc");
    window_ref = ((id (*)(id, SEL, CGRect, unsigned long, unsigned long, BOOL))objc_msgSend)(
        window_ref,
        selector("initWithContentRect:styleMask:backing:defer:"),
        rect(0, 0, 980, 650),
        NSWindowStyleMaskTitled |
            NSWindowStyleMaskClosable |
            NSWindowStyleMaskMiniaturizable |
            NSWindowStyleMaskResizable,
        NSBackingStoreBuffered,
        NO_VALUE
    );
    msg_void_id(window_ref, "setTitle:", ns_string("Learn Trideroah"));
    msg_void(window_ref, "center");

    scroll_ref = alloc_init_frame("NSScrollView", rect(0, 0, 980, 650));
    msg_void_bool(scroll_ref, "setHasVerticalScroller:", YES_VALUE);
    msg_void_bool(scroll_ref, "setDrawsBackground:", NO_VALUE);
    msg_void_id(window_ref, "setContentView:", scroll_ref);

    render_home(self);
    msg_void_id(window_ref, "makeKeyAndOrderFront:", NULL);
    ((void (*)(id, SEL, BOOL))objc_msgSend)(
        app,
        selector("activateIgnoringOtherApps:"),
        YES_VALUE
    );
}

static void application_did_finish_launching(id self, SEL _cmd, id notification) {
    (void)_cmd;
    (void)notification;
    build_window(self);
}

static BOOL should_terminate_after_last_window_closed(id self, SEL _cmd, id sender) {
    (void)self;
    (void)_cmd;
    (void)sender;
    return YES_VALUE;
}

static Class make_flipped_view_class(void) {
    Class superclass = objc_getClass("NSView");
    Class view_class = objc_allocateClassPair(superclass, "FlippedTrideroahView", 0);
    if (view_class) {
        class_addMethod(view_class, selector("isFlipped"), (IMP)should_terminate_after_last_window_closed, "c@:");
        objc_registerClassPair(view_class);
    }
    return objc_getClass("FlippedTrideroahView");
}

static Class make_delegate_class(void) {
    Class superclass = objc_getClass("NSObject");
    Class delegate_class = objc_allocateClassPair(superclass, "TrideroahLearnDelegate", 0);
    if (delegate_class) {
        class_addMethod(
            delegate_class,
            selector("applicationDidFinishLaunching:"),
            (IMP)application_did_finish_launching,
            "v@:@"
        );
        class_addMethod(
            delegate_class,
            selector("applicationShouldTerminateAfterLastWindowClosed:"),
            (IMP)should_terminate_after_last_window_closed,
            "c@:@"
        );
        class_addMethod(delegate_class, selector("checkPractice:"), (IMP)check_practice, "v@:@");
        class_addMethod(delegate_class, selector("nextPractice:"), (IMP)next_practice, "v@:@");
        class_addMethod(delegate_class, selector("showHome:"), (IMP)show_home, "v@:@");
        class_addMethod(delegate_class, selector("showLetters:"), (IMP)show_letters, "v@:@");
        class_addMethod(delegate_class, selector("showStats:"), (IMP)show_stats, "v@:@");
        class_addMethod(delegate_class, selector("startLetters:"), (IMP)start_letters, "v@:@");
        class_addMethod(delegate_class, selector("startWords:"), (IMP)start_words, "v@:@");
        objc_registerClassPair(delegate_class);
    }
    return objc_getClass("TrideroahLearnDelegate");
}

static void launch_learn_app(void) {
    srand((unsigned int)time(NULL));
    make_flipped_view_class();
    Class delegate_class = make_delegate_class();

    id app = msg_id(cls("NSApplication"), "sharedApplication");
    id delegate = msg_id(msg_id((id)delegate_class, "alloc"), "init");
    msg_void_id(app, "setDelegate:", delegate);
    msg_void_int(app, "setActivationPolicy:", NSApplicationActivationPolicyRegular);
    msg_void(app, "run");
}

static void print_usage(void) {
    fputs("Usage: trideroah learn\n", stderr);
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "learn") == 0) {
        resolve_asset_root(argv[0]);
        launch_learn_app();
        return 0;
    }

    print_usage();
    return argc == 1 ? 1 : 2;
}
