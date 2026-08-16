#define CLAY_IMPLEMENTATION
#include "clay.h"

double windowWidth = 1024, windowHeight = 768;
// Read by index.html to pick the HTML (0) or canvas (1) renderer. There's no
// switcher in the UI anymore, but the symbol is still exported from build.sh.
uint32_t ACTIVE_RENDERER_INDEX = 0;

// Font ids index into fontsById in index.html.
const uint32_t FONT_ID_MONO = 0;
const uint32_t FONT_ID_MONO_SEMI = 1;

// index.html multiplies every font size by GLOBAL_FONT_SCALING_FACTOR (0.8),
// so these are design pixels / 0.8. Every other measurement in this file is in
// design pixels, because text measurement comes back in CSS pixels.
#define FS_LEDE  22 // 18px
#define FS_TITLE 19 // 15px
#define FS_BODY  18 // 14px
#define FS_META  16 // 13px
#define FS_SMALL 15 // 12px
#define FS_LABEL 14 // 11px

// -- Lamplight palette -- semantic tokens for both themes --
typedef struct {
    Clay_Color bg;
    Clay_Color rule;        // hairlines between entries
    Clay_Color text;
    Clay_Color textMuted;
    Clay_Color textSubtle;
    Clay_Color accent;      // Lamplight: the selected tab, headings
    Clay_Color ember;       // interaction: active fills, control hovers
    Clay_Color madder;      // kept scarce - link hover, and nothing else
    Clay_Color onAccent;    // text sitting on an ember fill
    Clay_String iconGithub;
    Clay_String iconLinkedin;
    Clay_String iconResume;
    Clay_String toggleLabel; // names the theme you'd switch *to*
} Theme;

static const Theme THEMES[2] = {
    { // Dusk - the default
        .bg           = {  11,  39,  64, 255 }, // #0B2740 dusk-900
        .rule         = { 172, 208, 208,  46 }, // rgba(172,208,208,.18)
        .text         = { 251, 243, 222, 255 }, // #FBF3DE lamp-100
        .textMuted    = { 172, 208, 208, 255 }, // #ACD0D0 dusk-200
        .textSubtle   = { 134, 152, 150, 255 }, // #869896 stone-400
        .accent       = { 243, 210, 138, 255 }, // #F3D28A lamp-300
        .ember        = { 223, 162,  48, 255 }, // #DFA230 ember
        .madder       = { 200, 157, 146, 255 }, // #C89D92 rose-300
        .onAccent     = {  11,  39,  64, 255 }, // #0B2740 dusk-900
        .iconGithub   = CLAY_STRING_CONST("/clay/images/github_dusk.png"),
        .iconLinkedin = CLAY_STRING_CONST("/clay/images/linkedin_dusk.png"),
        .iconResume   = CLAY_STRING_CONST("/clay/images/resume_dusk.png"),
        .toggleLabel  = CLAY_STRING_CONST("paper"),
    },
    { // Paper
        .bg           = { 251, 247, 239, 255 }, // #FBF7EF paper
        .rule         = {  11,  39,  64,  36 }, // rgba(11,39,64,.14)
        .text         = {  11,  39,  64, 255 }, // #0B2740 dusk-900
        .textMuted    = {  43,  86, 109, 255 }, // #2B566D dusk-700
        .textSubtle   = { 110, 127, 134, 255 }, // #6E7F86 stone-600
        .accent       = { 122,  82,  23, 255 }, // #7A5217 lamp-700
        .ember        = { 176, 122,  46, 255 }, // #B07A2E lamp-600
        .madder       = { 140,  63,  71, 255 }, // #8C3F47 rose-600
        .onAccent     = { 251, 243, 222, 255 }, // #FBF3DE lamp-100
        .iconGithub   = CLAY_STRING_CONST("/clay/images/github_paper.png"),
        .iconLinkedin = CLAY_STRING_CONST("/clay/images/linkedin_paper.png"),
        .iconResume   = CLAY_STRING_CONST("/clay/images/resume_paper.png"),
        .toggleLabel  = CLAY_STRING_CONST("dusk"),
    },
};

uint32_t THEME_INDEX = 1;
#define T (&THEMES[THEME_INDEX])

// -- Content --------------------------------------------------------------

typedef enum {
    LANG_ALL = 0,
    LANG_JAVA,
    LANG_JAVASCRIPT,
    LANG_PYTHON,
    LANG_CPP,
    LANG_C,
    LANG_TYPESCRIPT,
    LANG_RACKET,
    LANG_COUNT
} Lang;

static const Clay_String FILTER_LABELS[LANG_COUNT] = {
    CLAY_STRING_CONST("All"),
    CLAY_STRING_CONST("Java"),
    CLAY_STRING_CONST("JavaScript"),
    CLAY_STRING_CONST("Python"),
    CLAY_STRING_CONST("C++"),
    CLAY_STRING_CONST("C"),
    CLAY_STRING_CONST("TypeScript"),
    CLAY_STRING_CONST("Racket"),
};

// How many of the chips above actually render. Only "All" for now - there are
// too few projects for language filtering to earn the width it was taking.
// The enum, the labels and every project's .lang stay intact, so raising this
// to LANG_COUNT brings the language chips straight back as the list grows.
#define FILTERS_SHOWN 1

// Thumbnails share a fixed width; height follows each image's own ratio, so
// nothing is cropped or letterboxed. 624px of row splits as 56 gutter + 24 + description + 24 + thumbnail, and
// the description is capped just below its share - so widening the thumbnail
// past ~140 trades directly against the text measure.
// The 888px row splits as 144 date gutter + 24 + 480 description + 24 + 210
// thumbnail. The gutter holds a full "May 2026 - Dec 2026" on one line, and
// the shell is wider than the source design's 704px because that design
// carried no images.
#define LOGO_HEIGHT  46
#define THUMB_WIDTH  210
#define THUMB_ASPECT (16.0f / 10.0f)

typedef struct {
    Clay_String year;
    Clay_String title;
    Clay_String desc;
    Clay_String stack;
    Clay_String url;
    Clay_String thumb;      // omit and the row renders without one
    float thumbAspect;      // width / height; 0 means 16:10
    uint32_t lang;
} Project;

static const Project PROJECTS[] = {
    {
        .year = CLAY_STRING_CONST("2026"),
        .title = CLAY_STRING_CONST("Personal Webpage in C"),
        .desc = CLAY_STRING_CONST("This page. Laid out by Clay in C and compiled to WebAssembly \xe2\x80\x94 no HTML markup, no CSS and no framework; the browser only ever receives rectangles and text."),
        .stack = CLAY_STRING_CONST("c \xc2\xb7 clay \xc2\xb7 wasm"),
        .url = CLAY_STRING_CONST("https://github.com/calvinKLyeung/calvinKLyeung.github.io"),
        .thumb = CLAY_STRING_CONST("/clay/images/self_in_c.png"),
        .lang = LANG_C,
    },
    {
        .year = CLAY_STRING_CONST("2026"),
        .title = CLAY_STRING_CONST("Christmas Tree Studio"),
        .desc = CLAY_STRING_CONST("A programmable 3D Christmas tree whose lights you drive by writing Python in the browser \xe2\x80\x94 Three.js renders the scene, Pyodide runs the interpreter client-side."),
        .stack = CLAY_STRING_CONST("TypeScript \xc2\xb7 Three.js \xc2\xb7 Python \xc2\xb7 Pyodide"),
        .url = CLAY_STRING_CONST("https://github.com/calvinKLyeung/christmas-tree-studio"),
        .thumb = CLAY_STRING_CONST("/clay/images/Christmas_Tree_Studio.png"),
        .lang = LANG_TYPESCRIPT,
    },
    {
        .year = CLAY_STRING_CONST("2025"),
        .title = CLAY_STRING_CONST("Racket Compiler"),
        .desc = CLAY_STRING_CONST("A nanopass compiler lowering a subset of Racket to x86-64, with higher-order functions, lexical scoping, macros and dynamic type checking. Graph-coloring register allocation cut generated code 83% and ran 10x faster."),
        .stack = CLAY_STRING_CONST("Racket \xc2\xb7 x86-64 \xc2\xb7 RackUnit"),
        .thumb = CLAY_STRING_CONST("/clay/images/racket_compiler.png"),
        // no .url — closed source, so no repo link
        .lang = LANG_RACKET,
    },
    {
        .year = CLAY_STRING_CONST("2024"),
        .title = CLAY_STRING_CONST("Course Planning Application"),
        .desc = CLAY_STRING_CONST("A multi-semester planner for optimizing course schedules and tracking degree progress, with a Swing interface for interaction and JSON files for persistence."),
        .stack = CLAY_STRING_CONST("Java \xc2\xb7 Swing \xc2\xb7 JUnit"),
        .url = CLAY_STRING_CONST("https://github.com/calvinKLyeung/CoursePlanningApp"),
        .thumb = CLAY_STRING_CONST("/clay/images/Course_Plan_App.png"),
        .thumbAspect = 320.0f / 240.0f,
        .lang = LANG_JAVA,
    },
    {
        .year = CLAY_STRING_CONST("2024"),
        .title = CLAY_STRING_CONST("HackMatch (nwPlus HackCamp 2024)"),
        .desc = CLAY_STRING_CONST("A hackathon front end pairing engineers with complementary collaborators, built as an interactive UI that filters and matches on technical expertise."),
        .stack = CLAY_STRING_CONST("JavaScript \xc2\xb7 HTML \xc2\xb7 CSS"),
        .url = CLAY_STRING_CONST("https://devpost.com/software/hackmatch-v3tlq8"),
        .thumb = CLAY_STRING_CONST("/clay/images/Hack_Match.png"),
        .thumbAspect = 356.0f / 200.0f,
        .lang = LANG_JAVASCRIPT,
    },
    {
        .year = CLAY_STRING_CONST("2023"),
        .title = CLAY_STRING_CONST("Mini B+ Tree Database"),
        .desc = CLAY_STRING_CONST("A small runtime database exposing CRUD over a hand-rolled B+ tree, holding O(log n) lookups and fast range scans under a tight memory budget."),
        .stack = CLAY_STRING_CONST("C++ \xc2\xb7 GoogleTest"),
        .url = CLAY_STRING_CONST("https://github.com/calvinKLyeung/BPlusTreeDBMS"),
        .thumb = CLAY_STRING_CONST("/clay/images/bplus_tree.png"),
        .lang = LANG_CPP,
    },
    {
        .year = CLAY_STRING_CONST("2023"),
        .title = CLAY_STRING_CONST("RSA Algorithm Exploration"),
        .desc = CLAY_STRING_CONST("RSA implemented from scratch \xe2\x80\x94 prime generation, modular exponentiation and key management \xe2\x80\x94 then benchmarked for security and speed across key sizes."),
        .stack = CLAY_STRING_CONST("Python \xc2\xb7 Jupyter Notebook"),
        .url = CLAY_STRING_CONST("https://github.com/calvinKLyeung/RSA-Algo-Exploration"),
        .thumb = CLAY_STRING_CONST("/clay/images/rsa.png"),
        .lang = LANG_PYTHON,
    },
};
#define PROJECT_COUNT (sizeof(PROJECTS) / sizeof(PROJECTS[0]))

typedef struct {
    Clay_String period;
    Clay_String role;
    Clay_String desc;
    Clay_String org;
    // Leave the logo fields off and the row renders without one. Supply the
    // reversed artwork for dusk and the standard one for paper.
    Clay_String url;        // optional: a paper, posting, or write-up
    Clay_String logoDusk;
    Clay_String logoPaper;
    float logoAspect; // width / height of the artwork
} Experience;

static const Experience EXPERIENCE[] = {
    {
        .period = CLAY_STRING_CONST("May 2026 \xe2\x80\x93 Dec 2026"),
        .role = CLAY_STRING_CONST("Junior Developer, Co-op"),
        .desc = CLAY_STRING_CONST("Maintained 1,770+ Java and Selenium regression tests across staging and production, and automated coverage for three storage integrations \xe2\x80\x94 saving 7 hours of manual testing per sprint and surfacing bugs a week earlier."),
        .org = CLAY_STRING_CONST("QA Automation, Jostle"),
        .logoDusk = CLAY_STRING_CONST("/clay/images/jostle_dusk.png"),
        .logoPaper = CLAY_STRING_CONST("/clay/images/jostle_paper.png"),
        .logoAspect = 288.0f / 74.0f,
    },
    {
        .period = CLAY_STRING_CONST("2019 \xe2\x80\x93 2023"),
        .role = CLAY_STRING_CONST("Research Assistant"),
        .desc = CLAY_STRING_CONST("Built Python tooling that extracts text and images from historical newspapers using BERT and YOLO, and automated quality control that improved digitization throughput 200%. Co-authored an IEEE PRAI 2023 paper."),
        .org = CLAY_STRING_CONST("Digital Initiatives, The Chinese University of Hong Kong Library"),
        .url = CLAY_STRING_CONST("https://doi.org/10.1109/PRAI59366.2023.10332028"),
        .logoDusk = CLAY_STRING_CONST("/clay/images/cuhk_dusk.png"),
        .logoPaper = CLAY_STRING_CONST("/clay/images/cuhk_paper.png"),
        .logoAspect = 758.0f / 193.0f,
    },
};
#define EXPERIENCE_COUNT (sizeof(EXPERIENCE) / sizeof(EXPERIENCE[0]))

typedef struct {
    Clay_String icon;   // optional; the text column stays aligned either way
    Clay_String text;
} Interest;

static const Interest INTERESTS[] = {
    { .icon = CLAY_STRING_CONST("\xf0\x9f\x93\xb7"),
      .text = CLAY_STRING_CONST("Walking cities with a camera \xe2\x80\x94 mostly Hong Kong, Japan, and Canada.") },
    { .icon = CLAY_STRING_CONST("\xf0\x9f\x8e\xb5"),
      .text = CLAY_STRING_CONST("Music with no discernible pattern: Cantopop, J-pop, classical, jazz, musicals, film scores.") },
    { .icon = CLAY_STRING_CONST("\xf0\x9f\x8e\xac"),
      .text = CLAY_STRING_CONST("Anime and film \xe2\x80\x94 lately Frieren, Perfect Days, Chungking Express.") },
    { .icon = CLAY_STRING_CONST("\xf0\x9f\x8e\xae"),
      .text = CLAY_STRING_CONST("Games, lighter than they used to be: years of Apex and Monster Hunter, now Pokémon Champions.") },
};
#define INTERESTS_COUNT (sizeof(INTERESTS) / sizeof(INTERESTS[0]))

// -- Frame arena ----------------------------------------------------------

typedef struct {
    void* memory;
    uintptr_t offset;
} Arena;

Arena frameArena = {};

typedef struct d {
    Clay_String link;
    bool cursorPointer;
    bool disablePointerEvents;
} CustomHTMLData;

CustomHTMLData* FrameAllocateCustomData(CustomHTMLData data) {
    CustomHTMLData *customData = (CustomHTMLData *)(frameArena.memory + frameArena.offset);
    *customData = data;
    frameArena.offset += sizeof(CustomHTMLData);
    return customData;
}

Clay_String* FrameAllocateString(Clay_String string) {
    Clay_String *allocated = (Clay_String *)(frameArena.memory + frameArena.offset);
    *allocated = string;
    frameArena.offset += sizeof(Clay_String);
    return allocated;
}

// -- Interaction ----------------------------------------------------------

uint32_t ACTIVE_FILTER = LANG_ALL;

typedef enum {
    SECTION_PROJECTS = 0,
    SECTION_EXPERIENCE,
    SECTION_INTERESTS,
    SECTION_COUNT
} Section;

static const Clay_String SECTION_LABELS[SECTION_COUNT] = {
    CLAY_STRING_CONST("Projects"),
    CLAY_STRING_CONST("Experience"),
    CLAY_STRING_CONST("Interests"),
};

uint32_t ACTIVE_SECTION = SECTION_PROJECTS;
// Switching tabs while scrolled down would otherwise strand you mid-section.
bool resetScrollNextFrame = false;

void HandleThemeToggle(Clay_ElementId elementId, Clay_PointerData pointerInfo, void *userData) {
    if (pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        THEME_INDEX = THEME_INDEX == 0 ? 1 : 0;
    }
}

void HandleFilterButton(Clay_ElementId elementId, Clay_PointerData pointerInfo, void *userData) {
    if (pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        ACTIVE_FILTER = (uint32_t)userData;
    }
}

void HandleTabButton(Clay_ElementId elementId, Clay_PointerData pointerInfo, void *userData) {
    if (pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME && ACTIVE_SECTION != (uint32_t)userData) {
        ACTIVE_SECTION = (uint32_t)userData;
        resetScrollNextFrame = true;
    }
}

// -- Components -----------------------------------------------------------

// Text that carries no pointer events, so hovering a parent still registers.
#define PASSTHROUGH FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })

// The three sections are peers: this row is the only heading on the page, and
// selecting a tab swaps the body beneath it.
void TabButton(uint32_t section) {
    bool active = ACTIVE_SECTION == section;
    CLAY(CLAY_IDI("Tab", section), {
        .layout = { .padding = { 0, 0, 0, 10 } },
        .backgroundColor = T->bg,
        .border = { .width = { .bottom = 2 }, .color = active ? T->ember : T->bg },
        .userData = FrameAllocateCustomData((CustomHTMLData) { .cursorPointer = true }),
    }) {
        bool hovered = Clay_Hovered();
        Clay_OnHover(HandleTabButton, (void *)section);
        CLAY_TEXT(SECTION_LABELS[section], CLAY_TEXT_CONFIG({
            .fontSize = FS_LABEL, .fontId = FONT_ID_MONO_SEMI, .letterSpacing = 2,
            .textColor = active ? T->accent : (hovered ? T->ember : T->textMuted),
            .userData = PASSTHROUGH }));
    }
}

void TabRow() {
    CLAY(CLAY_ID("TabRow"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0) }, .childGap = 24, .childAlignment = { .y = CLAY_ALIGN_Y_BOTTOM } },
        .border = { .width = { .bottom = 1 }, .color = T->rule },
    }) {
        for (uint32_t section = 0; section < SECTION_COUNT; section++) {
            TabButton(section);
        }
    }
}

// An icon + label pair that navigates on click, underlined like the rest of
// the links in the design.
void IconLink(int index, Clay_String label, Clay_String iconPath, Clay_String url) {
    CLAY(CLAY_IDI("IconLink", index), {
        .layout = { .childGap = 8, .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }, .padding = { 0, 0, 2, 3 } },
        .backgroundColor = T->bg,
        .border = { .width = { .bottom = 1 }, .color = Clay_Hovered() ? T->madder : T->rule },
        .userData = FrameAllocateCustomData((CustomHTMLData) { .link = url, .cursorPointer = true }),
    }) {
        bool hovered = Clay_Hovered();
        CLAY(CLAY_IDI("IconLinkImage", index), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(14) } },
            .aspectRatio = { 1 },
            .image = { .imageData = FrameAllocateString(iconPath) },
        }) {}
        CLAY_TEXT(label, CLAY_TEXT_CONFIG({
            .fontSize = FS_META, .fontId = FONT_ID_MONO,
            .textColor = hovered ? T->madder : T->textMuted,
            .userData = PASSTHROUGH }));
    }
}

void FilterButton(uint32_t lang) {
    bool active = ACTIVE_FILTER == lang;
    CLAY(CLAY_IDI("FilterButton", lang), {
        .layout = { .padding = { 7, 7, 2, 2 } },
        .backgroundColor = active ? T->ember : (Clay_Hovered() ? T->rule : T->bg),
        .userData = FrameAllocateCustomData((CustomHTMLData) { .cursorPointer = true }),
    }) {
        bool hovered = Clay_Hovered();
        Clay_OnHover(HandleFilterButton, (void *)lang);
        CLAY_TEXT(FILTER_LABELS[lang], CLAY_TEXT_CONFIG({
            .fontSize = FS_SMALL, .fontId = FONT_ID_MONO,
            .textColor = active ? T->onAccent : (hovered ? T->ember : T->textMuted),
            .userData = PASSTHROUGH }));
    }
}

// The shared three-column rhythm behind both the project and experience
// lists: a fixed date gutter, a flexible body, and a trailing slot holding
// either a link (projects) or a logo (experience). Both are optional - the
// description is capped at 380px, so the trailing slot sits in space the text
// never uses.
// A row carries either a logo (experience) or a thumbnail (projects), never
// both, so one aspect parameter serves whichever is present.
void EntryRow(Clay_ElementId id, bool mobileScreen, Clay_String gutter, Clay_String title, Clay_String desc, Clay_String meta, Clay_String linkLabel, Clay_String url, Clay_String logo, float imageAspect, Clay_String thumb, int index) {
    CLAY(id, {
        .layout = {
            .sizing = { CLAY_SIZING_GROW(0) },
            .layoutDirection = mobileScreen ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
            .childGap = mobileScreen ? 4 : 24,
            .padding = { 0, 0, 24, 24 },
        },
        .border = { .width = { .bottom = 1 }, .color = T->rule },
    }) {
        CLAY_AUTO_ID({ .layout = { .sizing = { mobileScreen ? CLAY_SIZING_FIT(0) : CLAY_SIZING_FIXED(144) }, .padding = { 0, 0, 3, 0 } } }) {
            CLAY_TEXT(gutter, CLAY_TEXT_CONFIG({ .fontSize = FS_SMALL, .fontId = FONT_ID_MONO, .textColor = T->textSubtle }));
        }
        CLAY_AUTO_ID({ .layout = { .sizing = { CLAY_SIZING_GROW(0) }, .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 6 } }) {
            CLAY_TEXT(title, CLAY_TEXT_CONFIG({ .fontSize = FS_TITLE, .fontId = FONT_ID_MONO_SEMI, .textColor = T->text }));
            CLAY_AUTO_ID({ .layout = { .sizing = { CLAY_SIZING_GROW(.max = 480) } } }) {
                CLAY_TEXT(desc, CLAY_TEXT_CONFIG({ .fontSize = FS_BODY, .fontId = FONT_ID_MONO, .lineHeight = 24, .textColor = T->textMuted }));
            }
            CLAY_TEXT(meta, CLAY_TEXT_CONFIG({ .fontSize = FS_SMALL, .fontId = FONT_ID_MONO, .textColor = T->textSubtle }));
        }
        // Artwork stacks above the link rather than beside it, so the pair
        // never has to share the row's spare width.
        bool showThumb = thumb.length > 0;
        bool showLogo = logo.length > 0;
        if (showThumb || showLogo || linkLabel.length > 0) {
            CLAY(CLAY_IDI("EntryTrailing", index), {
                .layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .childGap = 10,
                    // A lone logo rides the middle of the entry; a thumbnail
                    // stack stays aligned with the top of the text.
                    .sizing = { .height = showLogo ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIT(0) },
                    .childAlignment = { .x = mobileScreen ? CLAY_ALIGN_X_LEFT : CLAY_ALIGN_X_RIGHT,
                                        .y = showLogo ? CLAY_ALIGN_Y_CENTER : CLAY_ALIGN_Y_TOP },
                    .padding = { 0, 0, mobileScreen ? 14 : (showLogo ? 0 : 3), 0 },
                },
            }) {
                if (showThumb) {
                    CLAY(CLAY_IDI("EntryThumb", index), {
                        .layout = { .sizing = { CLAY_SIZING_FIXED(THUMB_WIDTH) } },
                        // The box takes the image's own ratio, so nothing is
                        // cropped or letterboxed. 0 falls back to 16:10.
                        .aspectRatio = { imageAspect > 0 ? imageAspect : THUMB_ASPECT },
                        .image = { .imageData = FrameAllocateString(thumb) },
                    }) {}
                }
                // Height-locked so logos of different proportions share a
                // baseline; width follows from the aspect ratio.
                if (showLogo) {
                    CLAY(CLAY_IDI("EntryLogo", index), {
                        .layout = { .sizing = { .height = CLAY_SIZING_FIXED(LOGO_HEIGHT) } },
                        .aspectRatio = { imageAspect },
                        .image = { .imageData = FrameAllocateString(logo) },
                    }) {}
                }
                if (linkLabel.length > 0) {
                    CLAY(CLAY_IDI("EntryLink", index), {
                        .layout = { .padding = { 0, 0, 0, 2 } },
                        .backgroundColor = T->bg,
                        .border = { .width = { .bottom = 1 }, .color = Clay_Hovered() ? T->madder : T->rule },
                        .userData = FrameAllocateCustomData((CustomHTMLData) { .link = url, .cursorPointer = true }),
                    }) {
                        CLAY_TEXT(linkLabel, CLAY_TEXT_CONFIG({
                            .fontSize = FS_SMALL, .fontId = FONT_ID_MONO,
                            .textColor = Clay_Hovered() ? T->madder : T->textMuted,
                            .userData = PASSTHROUGH }));
                    }
                }
            }
        }
    }
}

// -- Sections -------------------------------------------------------------

void HeaderBar() {
    CLAY(CLAY_ID("Header"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0) }, .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }, .padding = { 0, 0, 36, 0 } },
    }) {
        CLAY_TEXT(CLAY_STRING("Calvin Yeung"), CLAY_TEXT_CONFIG({ .fontSize = FS_BODY, .fontId = FONT_ID_MONO_SEMI, .textColor = T->text }));
        CLAY(CLAY_ID("HeaderSpacer"), { .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } }) {}
        CLAY(CLAY_ID("ThemeToggle"), {
            .layout = { .padding = { 0, 0, 2, 3 } },
            .backgroundColor = T->bg,
            .border = { .width = { .bottom = 1 }, .color = Clay_Hovered() ? T->ember : T->bg },
            .userData = FrameAllocateCustomData((CustomHTMLData) { .cursorPointer = true }),
        }) {
            bool hovered = Clay_Hovered();
            Clay_OnHover(HandleThemeToggle, 0);
            CLAY_TEXT(T->toggleLabel, CLAY_TEXT_CONFIG({
                .fontSize = FS_SMALL, .fontId = FONT_ID_MONO,
                .textColor = hovered ? T->ember : T->textMuted,
                .userData = PASSTHROUGH }));
        }
    }
}

void IntroSection(float gap) {
    CLAY(CLAY_ID("Intro"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0) }, .layoutDirection = CLAY_TOP_TO_BOTTOM, .padding = { 0, 0, (uint16_t)(gap * 1.1f), (uint16_t)gap } },
    }) {
        CLAY(CLAY_ID("IntroLede"), { .layout = { .sizing = { CLAY_SIZING_GROW(.max = 512) } } }) {
            CLAY_TEXT(CLAY_STRING("Came to computing from political science. Now at UBC, drawn to the layers underneath \xe2\x80\x94 networks, operating systems, databases, types, and language runtimes."), CLAY_TEXT_CONFIG({
                .fontSize = FS_LEDE, .fontId = FONT_ID_MONO, .lineHeight = 27, .textColor = T->text }));
        }
        CLAY(CLAY_ID("IntroSpacer"), { .layout = { .sizing = { .height = CLAY_SIZING_FIXED(32) } } }) {}
        CLAY(CLAY_ID("IntroLinks"), { .layout = { .childGap = 20, .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } } }) {
            IconLink(1, CLAY_STRING("GitHub"), T->iconGithub, CLAY_STRING("https://github.com/calvinKLyeung"));
            IconLink(2, CLAY_STRING("LinkedIn"), T->iconLinkedin, CLAY_STRING("https://www.linkedin.com/in/calvin-kin-lok-yeung"));
            // IconLink(3, CLAY_STRING("R\xc3\xa9sum\xc3\xa9"), T->iconResume, CLAY_STRING("/resume.pdf"));
        }
    }
}

void ProjectsSection(bool mobileScreen, float gap) {
    CLAY(CLAY_ID("Projects"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0) }, .layoutDirection = CLAY_TOP_TO_BOTTOM, .padding = { 0, 0, 0, (uint16_t)gap } },
    }) {
        CLAY(CLAY_ID("FilterRow"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(0) }, .childGap = 4, .padding = { 0, 0, 14, 6 } },
        }) {
            // Right-aligned on desktop, flush left once the row would crowd.
            if (!mobileScreen) {
                CLAY(CLAY_ID("FilterRowSpacer"), { .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } }) {}
            }
            for (uint32_t lang = LANG_ALL; lang < FILTERS_SHOWN; lang++) {
                FilterButton(lang);
            }
        }

        int shown = 0;
        for (uint32_t i = 0; i < PROJECT_COUNT; i++) {
            const Project *p = &PROJECTS[i];
            if (ACTIVE_FILTER != LANG_ALL && p->lang != ACTIVE_FILTER) {
                continue;
            }
            shown++;
            // Leave .url off a project (closed source, NDA, coursework) and
            // the repo link is omitted rather than rendered as a dead button.
            Clay_String repoLabel = p->url.length > 0 ? CLAY_STRING("repo") : (Clay_String) {};
            EntryRow(CLAY_IDI("Project", i), mobileScreen, p->year, p->title, p->desc, p->stack, repoLabel, p->url, (Clay_String) {}, p->thumbAspect, p->thumb, (int)i);
        }
        if (shown == 0) {
            CLAY(CLAY_ID("ProjectsEmpty"), { .layout = { .padding = { 0, 0, 32, 32 } } }) {
                CLAY_TEXT(CLAY_STRING("Nothing in that language yet."), CLAY_TEXT_CONFIG({
                    .fontSize = FS_META, .fontId = FONT_ID_MONO, .textColor = T->textSubtle }));
            }
        }
    }
}

void ExperienceSection(bool mobileScreen, float gap) {
    CLAY(CLAY_ID("Experience"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0) }, .layoutDirection = CLAY_TOP_TO_BOTTOM, .padding = { 0, 0, 0, (uint16_t)gap } },
    }) {
        CLAY(CLAY_ID("ExperienceTopSpacer"), { .layout = { .sizing = { .height = CLAY_SIZING_FIXED(20) } } }) {}
        for (uint32_t i = 0; i < EXPERIENCE_COUNT; i++) {
            const Experience *e = &EXPERIENCE[i];
            Clay_String logo = THEME_INDEX == 0 ? e->logoDusk : e->logoPaper;
            Clay_String paperLabel = e->url.length > 0 ? CLAY_STRING("paper") : (Clay_String) {};
            EntryRow(CLAY_IDI("Experience", i), mobileScreen, e->period, e->role, e->desc, e->org, paperLabel, e->url, logo, e->logoAspect, (Clay_String) {}, (int)(100 + i));
        }
    }
}

void InterestsSection(float gap) {
    CLAY(CLAY_ID("Interests"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0) }, .layoutDirection = CLAY_TOP_TO_BOTTOM, .padding = { 0, 0, 0, (uint16_t)gap } },
    }) {
        CLAY(CLAY_ID("InterestsBody"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(.max = 460) }, .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 14, .padding = { 0, 0, 26, 0 } },
        }) {
            for (uint32_t i = 0; i < INTERESTS_COUNT; i++) {
                // Icon in a fixed gutter, text in its own column, so a wrapped
                // second line lines up with the first rather than the icon.
                CLAY(CLAY_IDI("Interest", i), { .layout = { .sizing = { CLAY_SIZING_GROW(0) }, .childGap = 12 } }) {
                    CLAY_AUTO_ID({ .layout = { .sizing = { CLAY_SIZING_FIXED(22) } } }) {
                        CLAY_TEXT(INTERESTS[i].icon, CLAY_TEXT_CONFIG({
                            .fontSize = FS_BODY, .fontId = FONT_ID_MONO, .lineHeight = 24, .textColor = T->textMuted }));
                    }
                    CLAY_AUTO_ID({ .layout = { .sizing = { CLAY_SIZING_GROW(0) } } }) {
                        CLAY_TEXT(INTERESTS[i].text, CLAY_TEXT_CONFIG({
                            .fontSize = FS_BODY, .fontId = FONT_ID_MONO, .lineHeight = 24, .textColor = T->textMuted }));
                    }
                }
            }
        }
    }
}

void FooterBar() {
    CLAY(CLAY_ID("Footer"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0) }, .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }, .padding = { 0, 0, 20, 48 } },
        .border = { .width = { .top = 1 }, .color = T->rule },
    }) {
        CLAY_TEXT(CLAY_STRING("Vancouver, BC"), CLAY_TEXT_CONFIG({ .fontSize = FS_SMALL, .fontId = FONT_ID_MONO, .textColor = T->textSubtle }));
        CLAY(CLAY_ID("FooterSpacer"), { .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } }) {}
        CLAY(CLAY_ID("FooterSource"), {
            .layout = { .padding = { 0, 0, 2, 2 } },
            .backgroundColor = T->bg,
            .border = { .width = { .bottom = 1 }, .color = Clay_Hovered() ? T->text : T->rule },
            .userData = FrameAllocateCustomData((CustomHTMLData) { .link = CLAY_STRING("https://github.com/calvinKLyeung/calvinKLyeung.github.io"), .cursorPointer = true }),
        }) {
            CLAY_TEXT(CLAY_STRING("Source"), CLAY_TEXT_CONFIG({
                .fontSize = FS_SMALL, .fontId = FONT_ID_MONO,
                .textColor = Clay_Hovered() ? T->text : T->textSubtle,
                .userData = PASSTHROUGH }));
        }
    }
}

// -- Layout ---------------------------------------------------------------

typedef struct
{
    Clay_Vector2 clickOrigin;
    Clay_Vector2 positionOrigin;
    bool mouseDown;
} ScrollbarData;

ScrollbarData scrollbarData = (ScrollbarData) {};

Clay_RenderCommandArray CreateLayout(bool mobileScreen) {
    // clamp(1.5rem, 6vw, 2.5rem) and clamp(3rem, 8vw, 4.5rem) from the design.
    float edge = windowWidth * 0.06f;
    if (edge < 24) { edge = 24; } else if (edge > 40) { edge = 40; }
    float gap = windowWidth * 0.08f;
    if (gap < 48) { gap = 48; } else if (gap > 72) { gap = 72; }

    Clay_BeginLayout();
    CLAY(CLAY_ID("OuterContainer"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } }, .backgroundColor = T->bg }) {
        CLAY(CLAY_ID("OuterScrollContainer"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .layoutDirection = CLAY_TOP_TO_BOTTOM },
            .clip = { .vertical = true, .childOffset = Clay_GetScrollOffset() },
        }) {
            // Both grow to at least the viewport so FooterPush can hold the
            // footer at the bottom on short tabs, while still expanding past
            // it when a tab's content is taller.
            CLAY(CLAY_ID("Centerer"), { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER } } }) {
                CLAY(CLAY_ID("Shell"), {
                    .layout = {
                        .sizing = { CLAY_SIZING_GROW(.max = 968), CLAY_SIZING_GROW(0) },
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .padding = { (uint16_t)edge, (uint16_t)edge, 0, 0 },
                    },
                }) {
                    HeaderBar();
                    IntroSection(gap);
                    TabRow();
                    switch (ACTIVE_SECTION) {
                        case SECTION_EXPERIENCE: ExperienceSection(mobileScreen, gap); break;
                        case SECTION_INTERESTS:  InterestsSection(gap); break;
                        default:                 ProjectsSection(mobileScreen, gap); break;
                    }
                    CLAY(CLAY_ID("FooterPush"), { .layout = { .sizing = { .height = CLAY_SIZING_GROW(0) } } }) {}
                    FooterBar();
                }
            }
        }
    }

    if (!mobileScreen && ACTIVE_RENDERER_INDEX == 1) {
        Clay_ScrollContainerData scrollData = Clay_GetScrollContainerData(Clay_GetElementId(CLAY_STRING("OuterScrollContainer")));
        Clay_Color scrollbarColor = T->textMuted;
        scrollbarColor.a = 90;
        if (scrollbarData.mouseDown) {
            scrollbarColor.a = 180;
        } else if (Clay_PointerOver(Clay_GetElementId(CLAY_STRING("ScrollBar")))) {
            scrollbarColor.a = 140;
        }
        float scrollHeight = scrollData.scrollContainerDimensions.height - 12;
        CLAY(CLAY_ID("ScrollBar"), {
            .floating = { .offset = { .x = -6, .y = -(scrollData.scrollPosition->y / scrollData.contentDimensions.height) * scrollHeight + 6}, .zIndex = 1, .parentId = Clay_GetElementId(CLAY_STRING("OuterScrollContainer")).id, .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_TOP }, .attachTo = CLAY_ATTACH_TO_PARENT },
            .layout = { .sizing = {CLAY_SIZING_FIXED(10), CLAY_SIZING_FIXED((scrollHeight / scrollData.contentDimensions.height) * scrollHeight)} },
            .backgroundColor = scrollbarColor,
            .cornerRadius = CLAY_CORNER_RADIUS(5)
        }) {}
    }
    return Clay_EndLayout(0);
}

bool debugModeEnabled = false;

CLAY_WASM_EXPORT("SetScratchMemory") void SetScratchMemory(void * memory) {
    frameArena.memory = memory;
}

CLAY_WASM_EXPORT("UpdateDrawFrame") Clay_RenderCommandArray UpdateDrawFrame(float width, float height, float mouseWheelX, float mouseWheelY, float mousePositionX, float mousePositionY, bool isTouchDown, bool isMouseDown, bool arrowKeyDownPressedThisFrame, bool arrowKeyUpPressedThisFrame, bool dKeyPressedThisFrame, float deltaTime) {
    frameArena.offset = 0;
    windowWidth = width;
    windowHeight = height;
    Clay_SetLayoutDimensions((Clay_Dimensions) { width, height });
    Clay_ScrollContainerData scrollContainerData = Clay_GetScrollContainerData(Clay_GetElementId(CLAY_STRING("OuterScrollContainer")));

    if (resetScrollNextFrame) {
        if (scrollContainerData.scrollPosition) {
            scrollContainerData.scrollPosition->y = 0;
        }
        resetScrollNextFrame = false;
    }

    if (dKeyPressedThisFrame) {
        debugModeEnabled = !debugModeEnabled;
        Clay_SetDebugModeEnabled(debugModeEnabled);
    }
    Clay_SetCullingEnabled(ACTIVE_RENDERER_INDEX == 1);
    Clay_SetExternalScrollHandlingEnabled(ACTIVE_RENDERER_INDEX == 0);

    Clay__debugViewHighlightColor = (Clay_Color) {172, 208, 208, 120};

    Clay_SetPointerState((Clay_Vector2) {mousePositionX, mousePositionY}, isMouseDown || isTouchDown);

    if (!isMouseDown) {
        scrollbarData.mouseDown = false;
    }

    if (isMouseDown && !scrollbarData.mouseDown && Clay_PointerOver(Clay_GetElementId(CLAY_STRING("ScrollBar")))) {
        scrollbarData.clickOrigin = (Clay_Vector2) { mousePositionX, mousePositionY };
        scrollbarData.positionOrigin = *scrollContainerData.scrollPosition;
        scrollbarData.mouseDown = true;
    } else if (scrollbarData.mouseDown) {
        if (scrollContainerData.contentDimensions.height > 0) {
            Clay_Vector2 ratio = (Clay_Vector2) {
                scrollContainerData.contentDimensions.width / scrollContainerData.scrollContainerDimensions.width,
                scrollContainerData.contentDimensions.height / scrollContainerData.scrollContainerDimensions.height,
            };
            if (scrollContainerData.config.vertical) {
                scrollContainerData.scrollPosition->y = scrollbarData.positionOrigin.y + (scrollbarData.clickOrigin.y - mousePositionY) * ratio.y;
            }
            if (scrollContainerData.config.horizontal) {
                scrollContainerData.scrollPosition->x = scrollbarData.positionOrigin.x + (scrollbarData.clickOrigin.x - mousePositionX) * ratio.x;
            }
        }
    }

    if (arrowKeyDownPressedThisFrame) {
        if (scrollContainerData.contentDimensions.height > 0) {
            scrollContainerData.scrollPosition->y = scrollContainerData.scrollPosition->y - 50;
        }
    } else if (arrowKeyUpPressedThisFrame) {
        if (scrollContainerData.contentDimensions.height > 0) {
            scrollContainerData.scrollPosition->y = scrollContainerData.scrollPosition->y + 50;
        }
    }

    Clay_UpdateScrollContainers(isTouchDown, (Clay_Vector2) {mouseWheelX, mouseWheelY}, deltaTime);
    bool isMobileScreen = windowWidth < 608;
    if (debugModeEnabled) {
        isMobileScreen = windowWidth < 950;
    }
    return CreateLayout(isMobileScreen);
    //----------------------------------------------------------------------------------
}

// Dummy main() to please cmake - TODO get wasm working with cmake on this example
int main() {
    return 0;
}
