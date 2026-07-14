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
static id practice_image;
static id practice_prompt;
static id practice_answer;
static id practice_result;
static int current_index = 0;

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

static void add_subview(id parent, id child) {
    msg_void_id(parent, "addSubview:", child);
}

static void set_string(id field, const char *text) {
    msg_void_id(field, "setStringValue:", ns_string(text));
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

static void set_image_for_current_practice(void) {
    msg_void_id(practice_image, "setImage:", image_for_letter(letter_map[current_index].letter));
}

static void next_practice(id self, SEL _cmd, id sender) {
    (void)self;
    (void)_cmd;
    (void)sender;

    current_index = rand() % (int)(sizeof(letter_map) / sizeof(letter_map[0]));
    set_image_for_current_practice();

    char prompt[128];
    snprintf(
        prompt,
        sizeof(prompt),
        "Which English letter makes '%s'?",
        letter_map[current_index].token
    );
    set_string(practice_prompt, prompt);
    set_string(practice_answer, "");
    set_string(practice_result, "");
    msg_void_id(window_ref, "makeFirstResponder:", practice_answer);
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

    char expected = letter_map[current_index].letter[0];
    char actual = (char)tolower((unsigned char)answer[0]);
    char message[128];

    if (actual == expected) {
        snprintf(
            message,
            sizeof(message),
            "Correct: %c = %s",
            (char)toupper((unsigned char)expected),
            letter_map[current_index].token
        );
        msg_void_id(practice_result, "setTextColor:", color("systemGreenColor"));
    } else {
        snprintf(
            message,
            sizeof(message),
            "Answer: %c = %s",
            (char)toupper((unsigned char)expected),
            letter_map[current_index].token
        );
        msg_void_id(practice_result, "setTextColor:", color("systemRedColor"));
    }

    set_string(practice_result, message);
}

static id make_alphabet_tile(const char *letter, const char *token, double x, double y) {
    id box = alloc_init_frame("NSBox", rect(x, y, 140, 120));
    msg_void_id(box, "setTitle:", ns_string(""));

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

    id scroll = alloc_init_frame("NSScrollView", rect(0, 0, 980, 650));
    msg_void_bool(scroll, "setHasVerticalScroller:", YES_VALUE);
    msg_void_bool(scroll, "setDrawsBackground:", NO_VALUE);

    id root = alloc_init_frame("FlippedTrideroahView", rect(0, 0, 960, 760));
    msg_void_id(scroll, "setDocumentView:", root);
    msg_void_id(window_ref, "setContentView:", scroll);

    id title = make_label("Learn Trideroah", 24, 20, 500, 38, 30);
    add_subview(root, title);

    id subtitle = make_label(
        "Practice the glyphs, tokens, and English letters.",
        24,
        62,
        600,
        24,
        14
    );
    msg_void_id(subtitle, "setTextColor:", color("secondaryLabelColor"));
    add_subview(root, subtitle);

    id practice_heading = make_label("Practice", 24, 106, 200, 28, 20);
    add_subview(root, practice_heading);

    id practice_box = alloc_init_frame("NSBox", rect(24, 144, 920, 138));
    msg_void_id(practice_box, "setTitle:", ns_string(""));
    add_subview(root, practice_box);

    practice_image = alloc_init_frame("NSImageView", rect(20, 20, 96, 96));
    msg_void_int(practice_image, "setImageScaling:", NSImageScaleProportionallyUpOrDown);
    add_subview(practice_box, practice_image);

    practice_prompt = make_label("", 140, 22, 420, 26, 16);
    add_subview(practice_box, practice_prompt);

    practice_answer = alloc_init_frame("NSTextField", rect(140, 58, 220, 28));
    msg_void_id(practice_answer, "setPlaceholderString:", ns_string("Type the English letter"));
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

    id check_button = make_button("Check", self, selector("checkPractice:"), 140, 96);
    id next_button = make_button("Next", self, selector("nextPractice:"), 246, 96);
    add_subview(practice_box, check_button);
    add_subview(practice_box, next_button);

    practice_result = make_label("", 376, 60, 360, 24, 14);
    msg_void_id(practice_result, "setTextColor:", color("secondaryLabelColor"));
    add_subview(practice_box, practice_result);

    id alphabet_heading = make_label("Alphabet", 24, 320, 200, 28, 20);
    add_subview(root, alphabet_heading);

    for (int i = 0; i < 26; i++) {
        int column = i % 6;
        int row = i / 6;
        double x = 24 + column * 152;
        double y = 362 + row * 132;
        add_subview(root, make_alphabet_tile(letter_map[i].letter, letter_map[i].token, x, y));
    }

    next_practice(self, selector("nextPractice:"), NULL);
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
