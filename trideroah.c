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
    NSWindowStyleMaskFullScreen = 1 << 14,
    NSBackingStoreBuffered = 2,
    NSApplicationActivationPolicyRegular = 0,
    NSImageScaleProportionallyUpOrDown = 3,
    NSTextAlignmentCenter = 2,
    NSFocusRingTypeDefault = 0,
    NSViewWidthSizable = 1 << 1,
    NSViewHeightSizable = 1 << 4,
    NSWindowCollectionBehaviorFullScreenPrimary = 1 << 7
};

#define APP_WIDTH 2048.0
#define APP_HEIGHT 1280.0
#define BUTTON_HEIGHT 54.0

typedef struct {
    const char *letter;
    const char *token;
} LetterMap;

typedef enum {
    PRACTICE_NONE = 0,
    PRACTICE_LETTERS = 1,
    PRACTICE_WORDS = 2
} PracticeMode;

#define MAX_RESULTS 64
#define LETTER_SUITABLE_SECONDS 3.0
#define WORD_BASE_SUITABLE_SECONDS 1.5
#define WORD_SECONDS_PER_LETTER 0.75
#define MAX_WORD_LENGTH 12

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

static id window_ref;
static id scroll_ref;
static id practice_image;
static id practice_answer;
static int current_index = 0;
static char current_word[MAX_WORD_LENGTH + 1];
static char current_word_token[(MAX_WORD_LENGTH * 2) + 1];
static int word_length_min = 3;
static int word_length_max = 9;
static PracticeMode current_mode = PRACTICE_NONE;
static double question_started_at = 0.0;
static int letters_practiced = 0;
static int words_practiced = 0;
static double letter_times[MAX_RESULTS];
static double word_times[MAX_RESULTS];
static int letter_items[MAX_RESULTS];
static int word_items[MAX_RESULTS];
static int letter_failed[MAX_RESULTS];
static int word_failed[MAX_RESULTS];
static double word_thresholds[MAX_RESULTS];
static char word_labels[MAX_RESULTS][MAX_WORD_LENGTH + 1];
static int letter_time_count = 0;
static int word_time_count = 0;
static int awaiting_answer = 0;
static char last_user_answer[128];
static char last_correct_answer[128];
static int last_answer_correct = 0;

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

static CGRect view_bounds(id view) {
    return ((CGRect (*)(id, SEL))objc_msgSend)(view, selector("bounds"));
}

static void set_view_frame(id view, CGRect frame) {
    ((void (*)(id, SEL, CGRect))objc_msgSend)(view, selector("setFrame:"), frame);
}

static id font(double size) {
    return ((id (*)(id, SEL, double))objc_msgSend)(
        cls("NSFont"), selector("systemFontOfSize:"), size
    );
}

static id color(const char *name) {
    return msg_id(cls("NSColor"), name);
}

static id rgb_color(double red, double green, double blue, double alpha) {
    return ((id (*)(id, SEL, double, double, double, double))objc_msgSend)(
        cls("NSColor"),
        selector("colorWithCalibratedRed:green:blue:alpha:"),
        red,
        green,
        blue,
        alpha
    );
}

static const void *cg_color(const char *name) {
    return ((const void *(*)(id, SEL))objc_msgSend)(color(name), selector("CGColor"));
}

static const void *rgb_cg_color(double red, double green, double blue, double alpha) {
    return ((const void *(*)(id, SEL))objc_msgSend)(
        rgb_color(red, green, blue, alpha),
        selector("CGColor")
    );
}

static void set_draw_color(const char *name) {
    msg_void(color(name), "set");
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

static void store_result(
    double *times,
    int *items,
    int *failed,
    int *count,
    double value,
    int item,
    int did_fail
) {
    if (*count < MAX_RESULTS) {
        times[*count] = value;
        items[*count] = item;
        failed[*count] = did_fail;
        *count += 1;
        return;
    }

    for (int i = 1; i < MAX_RESULTS; i++) {
        times[i - 1] = times[i];
        items[i - 1] = items[i];
        failed[i - 1] = failed[i];
    }
    times[MAX_RESULTS - 1] = value;
    items[MAX_RESULTS - 1] = item;
    failed[MAX_RESULTS - 1] = did_fail;
}

static void store_word_result(double value, const char *word, double threshold, int did_fail) {
    int item = word_time_count < MAX_RESULTS ? word_time_count : MAX_RESULTS - 1;

    if (word_time_count >= MAX_RESULTS) {
        for (int i = 1; i < MAX_RESULTS; i++) {
            word_times[i - 1] = word_times[i];
            word_items[i - 1] = word_items[i];
            word_failed[i - 1] = word_failed[i];
            word_thresholds[i - 1] = word_thresholds[i];
            snprintf(word_labels[i - 1], sizeof(word_labels[i - 1]), "%s", word_labels[i]);
        }
    } else {
        word_time_count++;
    }

    word_times[item] = value;
    word_items[item] = item;
    word_failed[item] = did_fail;
    word_thresholds[item] = threshold;
    snprintf(word_labels[item], sizeof(word_labels[item]), "%s", word);
}

static double suitable_seconds_for_word(const char *word) {
    return WORD_BASE_SUITABLE_SECONDS + (double)strlen(word) * WORD_SECONDS_PER_LETTER;
}

static const char *token_for_letter_char(char letter) {
    int index = letter - 'a';
    if (index < 0 || index >= 26) {
        return "";
    }
    return letter_map[index].token;
}

static void encode_word_token(const char *word, char *buffer, size_t size) {
    buffer[0] = '\0';
    size_t used = 0;
    for (size_t i = 0; word[i] != '\0'; i++) {
        const char *token = token_for_letter_char(word[i]);
        int written = snprintf(buffer + used, size > used ? size - used : 0, "%s", token);
        if (written < 0) {
            break;
        }
        used += (size_t)written;
        if (used >= size) {
            buffer[size - 1] = '\0';
            break;
        }
    }
}

static void random_word(char *buffer, size_t size) {
    int min_length = word_length_min;
    int max_length = word_length_max;
    if (min_length < 1) {
        min_length = 1;
    }
    if (max_length < min_length) {
        max_length = min_length;
    }
    int length = min_length + rand() % (max_length - min_length + 1);
    if ((size_t)length >= size) {
        length = (int)size - 1;
    }
    for (int i = 0; i < length; i++) {
        buffer[i] = (char)('a' + rand() % 26);
    }
    buffer[length] = '\0';
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

static id make_colored_label(
    const char *text,
    double x,
    double y,
    double width,
    double height,
    double size,
    const char *color_name
) {
    id field = make_label(text, x, y, width, height, size);
    msg_void_id(field, "setTextColor:", color(color_name));
    return field;
}

static id make_button_sized(const char *title, id target, SEL action, double x, double y, double width) {
    id button = alloc_init_frame("NSButton", rect(x, y, width, BUTTON_HEIGHT));
    msg_void_id(button, "setTitle:", ns_string(title));
    msg_void_id(button, "setTarget:", target);
    ((void (*)(id, SEL, SEL))objc_msgSend)(button, selector("setAction:"), action);
    return button;
}

static id make_large_button_sized(const char *title, id target, SEL action, double x, double y, double width) {
    id button = alloc_init_frame("NSButton", rect(x, y, width, BUTTON_HEIGHT));
    msg_void_id(button, "setTitle:", ns_string(title));
    msg_void_id(button, "setTarget:", target);
    ((void (*)(id, SEL, SEL))objc_msgSend)(button, selector("setAction:"), action);
    return button;
}

static void style_card(id view) {
    msg_void_bool(view, "setWantsLayer:", YES_VALUE);
    id layer = msg_id(view, "layer");
    msg_void_ptr(layer, "setBackgroundColor:", rgb_cg_color(0.60, 0.60, 0.60, 1.0));
    msg_void_ptr(layer, "setBorderColor:", rgb_cg_color(0.60, 0.60, 0.60, 1.0));
    msg_void_double(layer, "setBorderWidth:", 1.0);
    msg_void_double(layer, "setCornerRadius:", 18.0);
}

static void style_page(id view) {
    msg_void_bool(view, "setWantsLayer:", YES_VALUE);
    id layer = msg_id(view, "layer");
    msg_void_ptr(layer, "setBackgroundColor:", rgb_cg_color(0.0, 0.0, 0.0, 1.0));
}

static void style_button_color(id button, double red, double green, double blue) {
    msg_void_bool(button, "setWantsLayer:", YES_VALUE);
    msg_void_bool(button, "setBordered:", NO_VALUE);
    id layer = msg_id(button, "layer");
    msg_void_ptr(layer, "setBackgroundColor:", rgb_cg_color(red, green, blue, 1.0));
    msg_void_double(layer, "setCornerRadius:", BUTTON_HEIGHT / 2.0);
    msg_void_bool(layer, "setMasksToBounds:", YES_VALUE);
}

static void style_button_text(id button, id text_color) {
    msg_void_id(button, "setContentTintColor:", text_color);
}

static void render_home(id self);
static void render_letters(id self);
static void render_stats(id self);
static void render_practice_screen(id self);
static void render_result_screen(id self, int correct);
static void render_finished_screen(id self);
static void next_practice(id self, SEL _cmd, id sender);
static void check_practice(id self, SEL _cmd, id sender);
static void end_practice(id self, SEL _cmd, id sender);
static void dont_know(id self, SEL _cmd, id sender);
static void start_words_any(id self, SEL _cmd, id sender);
static void start_words_short(id self, SEL _cmd, id sender);
static void start_words_medium(id self, SEL _cmd, id sender);
static void start_words_long(id self, SEL _cmd, id sender);

static id make_page(double height) {
    id page = alloc_init_frame("FlippedTrideroahView", rect(0, 0, APP_WIDTH, height));
    style_page(page);
    return page;
}

static void show_page(id page) {
    msg_void_bool(scroll_ref, "setHasVerticalScroller:", YES_VALUE);
    msg_void_bool(scroll_ref, "setHasHorizontalScroller:", NO_VALUE);
    msg_void_id(scroll_ref, "setDocumentView:", page);
}

static void show_focus_page(id page) {
    msg_void_bool(scroll_ref, "setHasVerticalScroller:", NO_VALUE);
    msg_void_bool(scroll_ref, "setHasHorizontalScroller:", NO_VALUE);
    CGRect bounds = view_bounds(scroll_ref);
    set_view_frame(page, rect(0, 0, bounds.size.width, bounds.size.height));
    msg_void_id(scroll_ref, "setDocumentView:", page);
}

static void add_tabs(id root, id self, int active_tab) {
    const char *titles[] = {"Home", "Letters", "Stats"};
    const char *actions[] = {"showHome:", "showLetters:", "showStats:"};

    for (int i = 0; i < 3; i++) {
        id button = make_button_sized(titles[i], self, selector(actions[i]), 24 + i * 112, 100, 100);
        style_button_color(button, i == active_tab ? 0.62 : 0.05, i == active_tab ? 0.62 : 0.05, i == active_tab ? 0.62 : 0.05);
        style_button_text(button, i == active_tab ? color("blackColor") : color("whiteColor"));
        add_subview(root, button);
    }
}

static void add_header(id root, id self, int active_tab) {
    id title = make_label("Learn Trideroah", 24, 20, 500, 38, 30);
    msg_void_id(title, "setTextColor:", color("whiteColor"));
    add_subview(root, title);

    id subtitle = make_label(
        "Practice the glyphs, tokens, English letters, and words.",
        24,
        62,
        680,
        24,
        14
    );
    msg_void_id(subtitle, "setTextColor:", rgb_color(0.65, 0.65, 0.65, 1.0));
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
    msg_void_id(title, "setTextColor:", color("whiteColor"));
    msg_void_int(title, "setAlignment:", NSTextAlignmentCenter);
    add_subview(box, title);

    return box;
}

static int pick_sticky_word(char *buffer, size_t size) {
    char labels[MAX_RESULTS][MAX_WORD_LENGTH + 1];
    double latest[MAX_RESULTS] = {0.0};
    int count = 0;

    for (int i = 0; i < word_time_count; i++) {
        int item = -1;
        for (int j = 0; j < count; j++) {
            if (strcmp(labels[j], word_labels[i]) == 0) {
                item = j;
                break;
            }
        }
        if (item < 0 && count < MAX_RESULTS) {
            item = count++;
            snprintf(labels[item], sizeof(labels[item]), "%s", word_labels[i]);
        }
        if (item >= 0) {
            latest[item] = word_times[i];
        }
    }

    int best = -1;
    double best_time = 0.0;
    for (int i = 0; i < count; i++) {
        double threshold = suitable_seconds_for_word(labels[i]);
        if (latest[i] > threshold && latest[i] > best_time) {
            best = i;
            best_time = latest[i];
        }
    }

    if (best < 0) {
        return 0;
    }

    snprintf(buffer, size, "%s", labels[best]);
    return 1;
}

static void generate_question(char *prompt, size_t prompt_size) {
    question_started_at = now_seconds();
    awaiting_answer = 1;

    if (current_mode == PRACTICE_WORDS) {
        if (rand() % 3 != 0 || !pick_sticky_word(current_word, sizeof(current_word))) {
            random_word(current_word, sizeof(current_word));
        }
        encode_word_token(current_word, current_word_token, sizeof(current_word_token));
        snprintf(
            prompt,
            prompt_size,
            "What English word makes '%s'?",
            current_word_token
        );
    } else {
        current_index = rand() % (int)(sizeof(letter_map) / sizeof(letter_map[0]));
        if (practice_image) {
            msg_void_id(practice_image, "setImage:", image_for_letter(letter_map[current_index].letter));
        }
        snprintf(
            prompt,
            prompt_size,
            "Which English letter makes '%s'?",
            letter_map[current_index].token
        );
    }
}

static void render_practice_screen(id self) {
    id root = make_page(APP_HEIGHT);
    CGRect bounds = view_bounds(scroll_ref);
    double page_width = bounds.size.width > 1.0 ? bounds.size.width : APP_WIDTH;
    double page_height = bounds.size.height > 1.0 ? bounds.size.height : APP_HEIGHT;
    set_view_frame(root, rect(0, 0, page_width, page_height));

    double panel_width = 640.0;
    double panel_height = 500.0;
    double left = (page_width - panel_width) / 2.0;
    double top = (page_height - panel_height) / 2.0;
    if (left < 24.0) {
        left = 24.0;
    }
    if (top < 80.0) {
        top = 80.0;
    }

    char prompt_text[256];
    generate_question(prompt_text, sizeof(prompt_text));

    id prompt = make_label(
        prompt_text,
        left,
        top,
        panel_width,
        34,
        22
    );
    msg_void_id(prompt, "setTextColor:", color("whiteColor"));
    msg_void_int(prompt, "setAlignment:", NSTextAlignmentCenter);
    add_subview(root, prompt);

    id image_box = alloc_init_frame("FlippedTrideroahView", rect(left, top + 90, 230, 230));
    msg_void_bool(image_box, "setWantsLayer:", YES_VALUE);
    id image_layer = msg_id(image_box, "layer");
    msg_void_ptr(image_layer, "setBackgroundColor:", rgb_cg_color(0.0, 0.0, 0.0, 1.0));
    msg_void_ptr(image_layer, "setBorderColor:", cg_color("whiteColor"));
    msg_void_double(image_layer, "setBorderWidth:", 4.0);
    msg_void_double(image_layer, "setCornerRadius:", 1.0);
    add_subview(root, image_box);

    if (current_mode == PRACTICE_LETTERS) {
        practice_image = alloc_init_frame("NSImageView", rect(30, 30, 170, 170));
        msg_void_int(practice_image, "setImageScaling:", NSImageScaleProportionallyUpOrDown);
        msg_void_id(practice_image, "setImage:", image_for_letter(letter_map[current_index].letter));
        add_subview(image_box, practice_image);
    } else {
        id token_label = make_label(current_word_token, 20, 92, 190, 46, 22);
        msg_void_id(token_label, "setTextColor:", color("whiteColor"));
        msg_void_int(token_label, "setAlignment:", NSTextAlignmentCenter);
        add_subview(image_box, token_label);
    }

    practice_answer = alloc_init_frame("NSTextField", rect(left + 310, top + 135, 330, 72));
    msg_void_id(
        practice_answer,
        "setPlaceholderString:",
        ns_string(current_mode == PRACTICE_WORDS ? "Answer goes here..." : "Answer goes here...")
    );
    msg_void_id(practice_answer, "setFont:", font(20));
    msg_void_id(practice_answer, "setTextColor:", color("whiteColor"));
    msg_void_id(practice_answer, "setBackgroundColor:", color("blackColor"));
    msg_void_bool(practice_answer, "setDrawsBackground:", YES_VALUE);
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
    add_subview(root, practice_answer);

    id check_button = make_large_button_sized("Check", self, selector("checkPractice:"), left, top + 360, 310);
    style_button_color(check_button, 0.0, 1.0, 0.0);
    style_button_text(check_button, color("blackColor"));
    add_subview(root, check_button);

    id dont_button = make_large_button_sized("Don't know", self, selector("dontKnow:"), left + 330, top + 360, 310);
    style_button_color(dont_button, 1.0, 0.12, 0.02);
    style_button_text(dont_button, color("whiteColor"));
    add_subview(root, dont_button);

    id exit_button = make_large_button_sized("Save and exit", self, selector("endPractice:"), left, top + 455, 640);
    style_button_color(exit_button, 0.05, 0.2, 1.0);
    style_button_text(exit_button, color("whiteColor"));
    add_subview(root, exit_button);

    show_focus_page(root);
    msg_void_id(window_ref, "makeFirstResponder:", practice_answer);
    msg_void_id(practice_answer, "selectText:", NULL);
}

static void render_result_screen(id self, int correct) {
    id root = make_page(APP_HEIGHT);
    CGRect bounds = view_bounds(scroll_ref);
    double page_width = bounds.size.width > 1.0 ? bounds.size.width : APP_WIDTH;
    double page_height = bounds.size.height > 1.0 ? bounds.size.height : APP_HEIGHT;
    set_view_frame(root, rect(0, 0, page_width, page_height));

    double panel_width = 360.0;
    double panel_height = correct ? 400.0 : 560.0;
    double left = (page_width - panel_width) / 2.0;
    double top = (page_height - panel_height) / 2.0;
    if (left < 24.0) {
        left = 24.0;
    }
    if (top < 80.0) {
        top = 80.0;
    }

    const char *status = correct ? "Correct" : "Incorrect";
    id status_label = make_label(status, left - 10, top, 380, 60, 34);
    msg_void_id(status_label, "setTextColor:", correct ? color("systemGreenColor") : color("systemRedColor"));
    msg_void_int(status_label, "setAlignment:", NSTextAlignmentCenter);
    add_subview(root, status_label);

    if (!correct) {
        id yours = make_label("Your answer:", left + 50, top + 95, 260, 32, 20);
        msg_void_id(yours, "setTextColor:", color("whiteColor"));
        msg_void_int(yours, "setAlignment:", NSTextAlignmentCenter);
        add_subview(root, yours);

        id answer_box = make_label(last_user_answer[0] ? last_user_answer : "(blank)", left + 40, top + 145, 280, 42, 20);
        msg_void_id(answer_box, "setTextColor:", rgb_color(0.65, 0.65, 0.65, 1.0));
        msg_void_int(answer_box, "setAlignment:", NSTextAlignmentCenter);
        add_subview(root, answer_box);

        id correct_title = make_label("Correct:", left + 80, top + 215, 200, 34, 20);
        msg_void_id(correct_title, "setTextColor:", color("whiteColor"));
        msg_void_int(correct_title, "setAlignment:", NSTextAlignmentCenter);
        add_subview(root, correct_title);
    }

    id correct_answer = make_label(last_correct_answer, left, top + (correct ? 110 : 270), 360, 40, 24);
    msg_void_id(correct_answer, "setTextColor:", color("systemGreenColor"));
    msg_void_int(correct_answer, "setAlignment:", NSTextAlignmentCenter);
    add_subview(root, correct_answer);

    id next_button = make_large_button_sized("Next ->", self, selector("nextPractice:"), left + 15, top + (correct ? 210 : 350), 330);
    style_button_color(next_button, 0.0, 1.0, 0.0);
    style_button_text(next_button, color("blackColor"));
    add_subview(root, next_button);

    id exit_button = make_large_button_sized("Exit", self, selector("endPractice:"), left + 15, top + (correct ? 310 : 455), 330);
    style_button_color(exit_button, 0.05, 0.2, 1.0);
    style_button_text(exit_button, color("whiteColor"));
    add_subview(root, exit_button);
    show_focus_page(root);
}

static void render_finished_screen(id self) {
    id root = make_page(APP_HEIGHT);
    CGRect bounds = view_bounds(scroll_ref);
    double page_width = bounds.size.width > 1.0 ? bounds.size.width : APP_WIDTH;
    double page_height = bounds.size.height > 1.0 ? bounds.size.height : APP_HEIGHT;
    set_view_frame(root, rect(0, 0, page_width, page_height));

    double left = (page_width - 440.0) / 2.0;
    double top = (page_height - 400.0) / 2.0;
    if (left < 24.0) {
        left = 24.0;
    }
    if (top < 80.0) {
        top = 80.0;
    }

    id label = make_label("Good Job!\nSee you\nnext time!", left, top, 440, 170, 34);
    msg_void_id(label, "setTextColor:", color("systemGreenColor"));
    msg_void_int(label, "setAlignment:", NSTextAlignmentCenter);
    add_subview(root, label);

    id home_button = make_large_button_sized("Go home", self, selector("showHome:"), left + 40, top + 260, 360);
    style_button_color(home_button, 0.0, 1.0, 0.0);
    style_button_text(home_button, color("blackColor"));
    add_subview(root, home_button);
    show_focus_page(root);
}

static void next_practice(id self, SEL _cmd, id sender) {
    (void)_cmd;
    (void)sender;
    render_practice_screen(self);
}

static void check_practice(id self, SEL _cmd, id sender) {
    (void)_cmd;
    (void)sender;

    id answer_string = msg_id(practice_answer, "stringValue");
    const char *answer = utf8_string(answer_string);
    while (*answer != '\0' && isspace((unsigned char)*answer)) {
        answer++;
    }
    snprintf(last_user_answer, sizeof(last_user_answer), "%s", answer);

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
        correct = strcmp(normalized, current_word) == 0;
        snprintf(
            message,
            sizeof(message),
            "%s: %s = %s",
            correct ? "Correct" : "Answer",
            current_word,
            current_word_token
        );
        snprintf(last_correct_answer, sizeof(last_correct_answer), "%s", current_word);
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
        snprintf(last_correct_answer, sizeof(last_correct_answer), "%c", (char)toupper((unsigned char)expected));
    }

    if (awaiting_answer) {
        double reaction = now_seconds() - question_started_at;
        if (current_mode == PRACTICE_WORDS) {
            words_practiced++;
            store_word_result(reaction, current_word, suitable_seconds_for_word(current_word), !correct);
        } else {
            letters_practiced++;
            store_result(
                letter_times,
                letter_items,
                letter_failed,
                &letter_time_count,
                reaction,
                current_index,
                !correct
            );
        }
        awaiting_answer = 0;
    }

    last_answer_correct = correct;
    (void)message;
    render_result_screen(self, correct);
}

static void dont_know(id self, SEL _cmd, id sender) {
    (void)_cmd;
    (void)sender;
    if (practice_answer) {
        set_string(practice_answer, "");
    }
    snprintf(last_user_answer, sizeof(last_user_answer), "Don't know");

    if (current_mode == PRACTICE_WORDS) {
        snprintf(last_correct_answer, sizeof(last_correct_answer), "%s", current_word);
    } else {
        snprintf(
            last_correct_answer,
            sizeof(last_correct_answer),
            "%c",
            (char)toupper((unsigned char)letter_map[current_index].letter[0])
        );
    }

    if (awaiting_answer) {
        double reaction = now_seconds() - question_started_at;
        if (current_mode == PRACTICE_WORDS) {
            words_practiced++;
            store_word_result(reaction, current_word, suitable_seconds_for_word(current_word), 1);
        } else {
            letters_practiced++;
            store_result(
                letter_times,
                letter_items,
                letter_failed,
                &letter_time_count,
                reaction,
                current_index,
                1
            );
        }
        awaiting_answer = 0;
    }
    last_answer_correct = 0;
    render_result_screen(self, 0);
}

static void add_alphabet_grid(id root, double start_y) {
    id alphabet_heading = make_label("Alphabet", 24, start_y, 200, 28, 20);
    msg_void_id(alphabet_heading, "setTextColor:", color("whiteColor"));
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
    id root = make_page(820);
    CGRect bounds = view_bounds(scroll_ref);
    double page_width = bounds.size.width > 1.0 ? bounds.size.width : APP_WIDTH;
    set_view_frame(root, rect(0, 0, page_width, 820));
    add_header(root, self, 0);

    double card_width = 430.0;
    double card_height = 164.0;
    double card_gap = 36.0;
    double content_left = (page_width - ((card_width * 2.0) + card_gap)) / 2.0;
    if (content_left < 24.0) {
        content_left = 24.0;
    }

    id heading = make_label("Home", content_left, 154, 300, 28, 20);
    msg_void_id(heading, "setTextColor:", color("whiteColor"));
    add_subview(root, heading);

    id letter_card = alloc_init_frame("FlippedTrideroahView", rect(content_left, 198, card_width, card_height));
    style_card(letter_card);
    add_subview(root, letter_card);
    id letter_title = make_label("Practice letters", 18, 16, 240, 26, 18);
    msg_void_id(letter_title, "setTextColor:", color("whiteColor"));
    add_subview(letter_card, letter_title);
    id letter_desc = make_label("See a glyph and token, then type the matching English letter.", 18, 48, 360, 44, 14);
    msg_void_id(letter_desc, "setTextColor:", color("secondaryLabelColor"));
    add_subview(letter_card, letter_desc);
    id letter_button = make_button_sized("Go", self, selector("startLetters:"), 260, 96, 140);
    style_button_color(letter_button, 0.05, 0.2, 1.0);
    style_button_text(letter_button, color("whiteColor"));
    add_subview(letter_card, letter_button);

    id word_card = alloc_init_frame("FlippedTrideroahView", rect(content_left + card_width + card_gap, 198, card_width, card_height));
    style_card(word_card);
    add_subview(root, word_card);
    id word_title = make_label("Practice words", 18, 16, 240, 26, 18);
    msg_void_id(word_title, "setTextColor:", color("whiteColor"));
    add_subview(word_card, word_title);
    id word_desc = make_label("Read a Trideroah word token and type the English word.", 18, 48, 340, 44, 14);
    msg_void_id(word_desc, "setTextColor:", color("secondaryLabelColor"));
    add_subview(word_card, word_desc);

    double button_left = 18.0;
    double button_gap = 10.0;
    double button_width = (card_width - (button_left * 2.0) - (button_gap * 3.0)) / 4.0;
    id any_button = make_button_sized("Any", self, selector("startWordsAny:"), button_left, 106, button_width);
    id short_button = make_button_sized("Short", self, selector("startWordsShort:"), button_left + (button_width + button_gap), 106, button_width);
    id medium_button = make_button_sized("Medium", self, selector("startWordsMedium:"), button_left + ((button_width + button_gap) * 2.0), 106, button_width);
    id long_button = make_button_sized("Long", self, selector("startWordsLong:"), button_left + ((button_width + button_gap) * 3.0), 106, button_width);
    style_button_color(any_button, 0.05, 0.2, 1.0);
    style_button_color(short_button, 0.05, 0.2, 1.0);
    style_button_color(medium_button, 0.05, 0.2, 1.0);
    style_button_color(long_button, 0.05, 0.2, 1.0);
    style_button_text(any_button, color("whiteColor"));
    style_button_text(short_button, color("whiteColor"));
    style_button_text(medium_button, color("whiteColor"));
    style_button_text(long_button, color("whiteColor"));
    add_subview(word_card, any_button);
    add_subview(word_card, short_button);
    add_subview(word_card, medium_button);
    add_subview(word_card, long_button);

    show_page(root);
}

static void render_letters(id self) {
    id root = make_page(1040);
    add_header(root, self, 1);
    add_alphabet_grid(root, 154);
    show_page(root);
}

static id bezier_path(void) {
    return msg_id(cls("NSBezierPath"), "bezierPath");
}

static void path_move(id path, double x, double y) {
    ((void (*)(id, SEL, CGPoint))objc_msgSend)(path, selector("moveToPoint:"), (CGPoint){x, y});
}

static void path_line(id path, double x, double y) {
    ((void (*)(id, SEL, CGPoint))objc_msgSend)(path, selector("lineToPoint:"), (CGPoint){x, y});
}

static void path_line_width(id path, double width) {
    msg_void_double(path, "setLineWidth:", width);
}

static void path_dash(id path, double first, double second) {
    double pattern[2] = {first, second};
    ((void (*)(id, SEL, const double *, long, double))objc_msgSend)(
        path,
        selector("setLineDash:count:phase:"),
        pattern,
        2,
        0.0
    );
}

static void path_stroke(id path) {
    msg_void(path, "stroke");
}

static void fill_oval(double x, double y, double width, double height) {
    id oval = ((id (*)(id, SEL, CGRect))objc_msgSend)(
        cls("NSBezierPath"),
        selector("bezierPathWithOvalInRect:"),
        rect(x, y, width, height)
    );
    msg_void(oval, "fill");
}

static double graph_width_for_count(int count) {
    double width = 760.0;
    double expanded = 180.0 + (double)(count > 1 ? count : 1) * 90.0;
    return expanded > width ? expanded : width;
}

static void draw_line_graph(int graph_kind) {
    double *times = graph_kind == 1 ? letter_times : word_times;
    int *failed = graph_kind == 1 ? letter_failed : word_failed;
    int count = graph_kind == 1 ? letter_time_count : word_time_count;
    double left = 84.0;
    double top = 34.0;
    double bottom = 244.0;
    double graph_width = graph_width_for_count(count);
    double right = graph_width - 50.0;
    double plot_height = bottom - top;

    set_draw_color("blackColor");
    id background = ((id (*)(id, SEL, CGRect))objc_msgSend)(
        cls("NSBezierPath"),
        selector("bezierPathWithRect:"),
        rect(0, 0, graph_width, 300)
    );
    msg_void(background, "fill");

    set_draw_color("whiteColor");
    id axes = bezier_path();
    path_line_width(axes, 4.0);
    path_move(axes, left, top);
    path_line(axes, left, bottom);
    path_line(axes, right, bottom);
    path_stroke(axes);

    double now_x = right - 70.0;
    set_draw_color("systemRedColor");
    id now_line = bezier_path();
    path_line_width(now_line, 4.0);
    path_dash(now_line, 10.0, 8.0);
    path_move(now_line, now_x, top);
    path_line(now_line, now_x, bottom);
    path_stroke(now_line);

    if (count == 0) {
        return;
    }

    int start = 0;
    int visible = count - start;
    double max_time = graph_kind == 1 ? LETTER_SUITABLE_SECONDS : 1.0;
    for (int i = start; i < count; i++) {
        if (times[i] > max_time) {
            max_time = times[i];
        }
        double threshold = graph_kind == 1 ? LETTER_SUITABLE_SECONDS : word_thresholds[i];
        if (threshold > max_time) {
            max_time = threshold;
        }
    }

    double graph_max = max_time * 1.15;
    if (graph_max < 1.0) {
        graph_max = 1.0;
    }

    set_draw_color("systemGreenColor");
    id threshold_line = bezier_path();
    path_line_width(threshold_line, 3.0);
    path_dash(threshold_line, 8.0, 8.0);
    for (int i = 0; i < visible; i++) {
        double fraction = visible == 1 ? 1.0 : (double)i / (double)(visible - 1);
        double x = left + fraction * (now_x - left);
        double threshold = graph_kind == 1 ? LETTER_SUITABLE_SECONDS : word_thresholds[start + i];
        double value = clamp_double(threshold / graph_max, 0.0, 1.0);
        double y = bottom - value * plot_height;
        if (i == 0) {
            path_move(threshold_line, x, y);
        } else {
            path_line(threshold_line, x, y);
        }
    }
    path_stroke(threshold_line);

    set_draw_color("systemBlueColor");
    id line = bezier_path();
    path_line_width(line, 5.0);

    for (int i = 0; i < visible; i++) {
        double fraction = visible == 1 ? 1.0 : (double)i / (double)(visible - 1);
        double x = left + fraction * (now_x - left);
        double value = clamp_double(times[start + i] / graph_max, 0.0, 1.0);
        double y = bottom - value * plot_height;

        if (i == 0) {
            path_move(line, x, y);
        } else {
            path_line(line, x, y);
        }
    }
    path_stroke(line);

    for (int i = 0; i < visible; i++) {
        double fraction = visible == 1 ? 1.0 : (double)i / (double)(visible - 1);
        double x = left + fraction * (now_x - left);
        double value = clamp_double(times[start + i] / graph_max, 0.0, 1.0);
        double y = bottom - value * plot_height;
        set_draw_color(failed[start + i] ? "systemRedColor" : "systemBlueColor");
        fill_oval(x - 10.0, y - 10.0, 20.0, 20.0);
    }
}

static void draw_letter_graph(id self, SEL _cmd, CGRect dirty_rect) {
    (void)self;
    (void)_cmd;
    (void)dirty_rect;
    draw_line_graph(1);
}

static void draw_word_graph(id self, SEL _cmd, CGRect dirty_rect) {
    (void)self;
    (void)_cmd;
    (void)dirty_rect;
    draw_line_graph(2);
}

static void sticky_text_for_kind(int graph_kind, char *buffer, size_t size) {
    double *times = graph_kind == 1 ? letter_times : word_times;
    int count = graph_kind == 1 ? letter_time_count : word_time_count;
    double latest_times[MAX_RESULTS] = {0.0};
    int latest_seen[MAX_RESULTS] = {0};
    char latest_labels[MAX_RESULTS][MAX_WORD_LENGTH + 1];
    int item_count = 0;

    if (graph_kind == 1) {
        for (int i = 0; i < count; i++) {
            int item = letter_items[i];
            if (item >= 0 && item < 26) {
                latest_seen[item] = 1;
                latest_times[item] = times[i];
            }
        }
        item_count = 26;
    } else {
        for (int i = 0; i < count; i++) {
            int item = -1;
            for (int j = 0; j < item_count; j++) {
                if (strcmp(latest_labels[j], word_labels[i]) == 0) {
                    item = j;
                    break;
                }
            }
            if (item < 0 && item_count < MAX_RESULTS) {
                item = item_count++;
                snprintf(latest_labels[item], sizeof(latest_labels[item]), "%s", word_labels[i]);
            }
            if (item >= 0) {
                latest_seen[item] = 1;
                latest_times[item] = times[i];
            }
        }
    }

    int chosen[5] = {-1, -1, -1, -1, -1};
    for (int slot = 0; slot < 5; slot++) {
        int best = -1;
        double best_time = 0.0;
        for (int i = 0; i < item_count; i++) {
            int already_chosen = 0;
            for (int j = 0; j < slot; j++) {
                if (chosen[j] == i) {
                    already_chosen = 1;
                }
            }
            const char *label = graph_kind == 1 ? letter_map[i].letter : latest_labels[i];
            double threshold = graph_kind == 1 ? LETTER_SUITABLE_SECONDS : suitable_seconds_for_word(label);
            if (!already_chosen && latest_seen[i] && latest_times[i] > threshold && latest_times[i] > best_time) {
                best = i;
                best_time = latest_times[i];
            }
        }
        chosen[slot] = best;
    }

    snprintf(buffer, size, "Sticky: ");
    size_t used = strlen(buffer);
    int added = 0;
    for (int i = 0; i < 5 && chosen[i] >= 0; i++) {
        const char *label = graph_kind == 1
            ? letter_map[chosen[i]].letter
            : latest_labels[chosen[i]];
        int written = snprintf(
            buffer + used,
            size > used ? size - used : 0,
            "%s%s",
            added ? ", " : "",
            label
        );
        if (written < 0) {
            break;
        }
        used += (size_t)written;
        added++;
    }

    if (!added) {
        snprintf(buffer, size, "Sticky: none");
    }
}

static void add_graph(
    id root,
    const char *title,
    double *times,
    int count,
    double y,
    const char *graph_class,
    int graph_kind
) {
    add_subview(root, make_label(title, 24, y, 300, 26, 18));

    id card = alloc_init_frame("FlippedTrideroahView", rect(24, y + 40, 780, 320));
    style_card(card);
    add_subview(root, card);

    id graph_scroll = alloc_init_frame("NSScrollView", rect(10, 10, 760, 300));
    msg_void_bool(graph_scroll, "setHasHorizontalScroller:", YES_VALUE);
    msg_void_bool(graph_scroll, "setHasVerticalScroller:", YES_VALUE);
    msg_void_bool(graph_scroll, "setDrawsBackground:", NO_VALUE);
    add_subview(card, graph_scroll);

    double graph_width = graph_width_for_count(count);
    id graph = alloc_init_frame(graph_class, rect(0, 0, graph_width, 300));
    msg_void_id(graph_scroll, "setDocumentView:", graph);

    double display_threshold = graph_kind == 1 ? LETTER_SUITABLE_SECONDS : 1.0;
    double max_time = display_threshold;
    for (int i = 0; i < count; i++) {
        if (times[i] > max_time) {
            max_time = times[i];
        }
        double item_threshold = graph_kind == 1 ? LETTER_SUITABLE_SECONDS : word_thresholds[i];
        if (item_threshold > max_time) {
            max_time = item_threshold;
        }
        display_threshold = item_threshold;
    }
    max_time *= 1.15;

    char max_label[32];
    char mid_label[32];
    snprintf(max_label, sizeof(max_label), "%.1fs", max_time);
    snprintf(mid_label, sizeof(mid_label), "%.1fs", max_time / 2.0);

    add_subview(card, make_colored_label(max_label, 18, 34, 58, 24, 14, "whiteColor"));
    add_subview(card, make_colored_label(mid_label, 18, 136, 58, 24, 14, "whiteColor"));
    add_subview(card, make_colored_label("0s", 34, 242, 42, 24, 14, "whiteColor"));
    add_subview(card, make_colored_label("Start", 104, 264, 80, 24, 14, "whiteColor"));
    add_subview(card, make_colored_label("Now", 672, 264, 80, 24, 14, "systemRedColor"));
    double threshold_value = clamp_double(display_threshold / max_time, 0.0, 1.0);
    double suitable_y = 10.0 + 244.0 - threshold_value * (244.0 - 34.0) - 12.0;
    add_subview(card, make_colored_label("Suitable", 612, suitable_y, 100, 24, 13, "systemGreenColor"));

    if (count == 0) {
        id empty = make_label("No answers yet.", 330, y + 176, 180, 24, 14);
        msg_void_id(empty, "setTextColor:", color("secondaryLabelColor"));
        add_subview(root, empty);
    }

    char sticky[256];
    sticky_text_for_kind(graph_kind, sticky, sizeof(sticky));
    id sticky_label = make_label(sticky, 24, y + 372, 760, 24, 15);
    msg_void_id(sticky_label, "setTextColor:", color("secondaryLabelColor"));
    add_subview(root, sticky_label);
}

static void render_stats(id self) {
    id root = make_page(1120);
    add_header(root, self, 2);

    id stats_title = make_label("Stats", 24, 154, 300, 28, 20);
    msg_void_id(stats_title, "setTextColor:", color("whiteColor"));
    add_subview(root, stats_title);

    char letter_text[128];
    char word_text[128];
    snprintf(letter_text, sizeof(letter_text), "Letters practiced: %d", letters_practiced);
    snprintf(word_text, sizeof(word_text), "Words practiced: %d", words_practiced);

    id letter_count = make_label(letter_text, 24, 202, 260, 26, 18);
    id word_count = make_label(word_text, 310, 202, 260, 26, 18);
    msg_void_id(letter_count, "setTextColor:", color("whiteColor"));
    msg_void_id(word_count, "setTextColor:", color("whiteColor"));
    add_subview(root, letter_count);
    add_subview(root, word_count);

    add_graph(root, "Letter reaction time", letter_times, letter_time_count, 264, "TrideroahLetterGraphView", 1);
    add_graph(root, "Word reaction time", word_times, word_time_count, 676, "TrideroahWordGraphView", 2);
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
    render_practice_screen(self);
}

static void start_words_with_length(id self, int min_length, int max_length) {
    word_length_min = min_length;
    word_length_max = max_length;
    current_mode = PRACTICE_WORDS;
    render_practice_screen(self);
}

static void start_words_any(id self, SEL _cmd, id sender) {
    (void)_cmd;
    (void)sender;
    start_words_with_length(self, 3, 9);
}

static void start_words_short(id self, SEL _cmd, id sender) {
    (void)_cmd;
    (void)sender;
    start_words_with_length(self, 3, 4);
}

static void start_words_medium(id self, SEL _cmd, id sender) {
    (void)_cmd;
    (void)sender;
    start_words_with_length(self, 5, 7);
}

static void start_words_long(id self, SEL _cmd, id sender) {
    (void)_cmd;
    (void)sender;
    start_words_with_length(self, 8, 12);
}

static void end_practice(id self, SEL _cmd, id sender) {
    (void)_cmd;
    (void)sender;
    current_mode = PRACTICE_NONE;
    awaiting_answer = 0;
    render_finished_screen(self);
}

static void build_window(id self) {
    id app = msg_id(cls("NSApplication"), "sharedApplication");

    window_ref = msg_id(cls("NSWindow"), "alloc");
    window_ref = ((id (*)(id, SEL, CGRect, unsigned long, unsigned long, BOOL))objc_msgSend)(
        window_ref,
        selector("initWithContentRect:styleMask:backing:defer:"),
        rect(0, 0, APP_WIDTH, APP_HEIGHT),
        NSWindowStyleMaskTitled |
            NSWindowStyleMaskClosable |
            NSWindowStyleMaskMiniaturizable |
            NSWindowStyleMaskResizable |
            NSWindowStyleMaskFullScreen,
        NSBackingStoreBuffered,
        NO_VALUE
    );
    msg_void_id(window_ref, "setTitle:", ns_string("Learn Trideroah"));
    msg_void_int(window_ref, "setCollectionBehavior:", NSWindowCollectionBehaviorFullScreenPrimary);
    msg_void(window_ref, "center");

    scroll_ref = alloc_init_frame("NSScrollView", rect(0, 0, APP_WIDTH, APP_HEIGHT));
    msg_void_int(scroll_ref, "setAutoresizingMask:", NSViewWidthSizable | NSViewHeightSizable);
    msg_void_bool(scroll_ref, "setHasVerticalScroller:", YES_VALUE);
    msg_void_bool(scroll_ref, "setHasHorizontalScroller:", NO_VALUE);
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

static void make_graph_view_classes(void) {
    Class superclass = objc_getClass("NSView");
    Class letter_class = objc_allocateClassPair(superclass, "TrideroahLetterGraphView", 0);
    if (letter_class) {
        class_addMethod(letter_class, selector("isFlipped"), (IMP)should_terminate_after_last_window_closed, "c@:");
        class_addMethod(letter_class, selector("drawRect:"), (IMP)draw_letter_graph, "v@:{CGRect={CGPoint=dd}{CGSize=dd}}");
        objc_registerClassPair(letter_class);
    }

    Class word_class = objc_allocateClassPair(superclass, "TrideroahWordGraphView", 0);
    if (word_class) {
        class_addMethod(word_class, selector("isFlipped"), (IMP)should_terminate_after_last_window_closed, "c@:");
        class_addMethod(word_class, selector("drawRect:"), (IMP)draw_word_graph, "v@:{CGRect={CGPoint=dd}{CGSize=dd}}");
        objc_registerClassPair(word_class);
    }
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
        class_addMethod(delegate_class, selector("dontKnow:"), (IMP)dont_know, "v@:@");
        class_addMethod(delegate_class, selector("nextPractice:"), (IMP)next_practice, "v@:@");
        class_addMethod(delegate_class, selector("showHome:"), (IMP)show_home, "v@:@");
        class_addMethod(delegate_class, selector("showLetters:"), (IMP)show_letters, "v@:@");
        class_addMethod(delegate_class, selector("showStats:"), (IMP)show_stats, "v@:@");
        class_addMethod(delegate_class, selector("startLetters:"), (IMP)start_letters, "v@:@");
        class_addMethod(delegate_class, selector("startWordsAny:"), (IMP)start_words_any, "v@:@");
        class_addMethod(delegate_class, selector("startWordsShort:"), (IMP)start_words_short, "v@:@");
        class_addMethod(delegate_class, selector("startWordsMedium:"), (IMP)start_words_medium, "v@:@");
        class_addMethod(delegate_class, selector("startWordsLong:"), (IMP)start_words_long, "v@:@");
        class_addMethod(delegate_class, selector("endPractice:"), (IMP)end_practice, "v@:@");
        objc_registerClassPair(delegate_class);
    }
    return objc_getClass("TrideroahLearnDelegate");
}

static void launch_learn_app(void) {
    srand((unsigned int)time(NULL));
    make_flipped_view_class();
    make_graph_view_classes();
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
