#include "wheel/interface_catalog.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/** @brief Internal dimensions, packet identifiers, and mode selectors for catalog transfers. */
enum {
    INTERFACE_CATALOG_RECORD_COUNT = 26,              /**< Number of binary catalog records. */
    INTERFACE_CATALOG_RECORD_SIZE = 21,               /**< Size of one binary catalog record. */
    INTERFACE_CATALOG_PAGE_COUNT = 26,                /**< Number of indexed-help pages. */
    INTERFACE_CATALOG_SECTION_COUNT = 7,              /**< Number of sections per help page. */
    INTERFACE_CATALOG_CHUNK_SIZE = 27,                /**< Maximum text bytes per response chunk. */
    INTERFACE_CATALOG_REMOTE_PACKET_TYPE = 0x80,      /**< Binary catalog packet type. */
    INTERFACE_CATALOG_CONFIGURATION_REPORT_ID = 0xa6, /**< Indexed-help report identifier. */
    INTERFACE_CATALOG_CONFIGURATION_PACKET_TYPE = 0x81, /**< Indexed-help packet type. */
    INTERFACE_CATALOG_LEGACY_WHEEL_MODE = 0x0e,        /**< Legacy catalog output mode. */
    INTERFACE_CATALOG_EXTENDED_WHEEL_MODE = 0x1c,       /**< Extended remote-tuning wheel mode. */
};

/** @brief One binary-backed tuning-help text field. */
typedef struct {
    const char *text; /**< Text bytes, without a terminator in the transfer. */
    uint8_t length;   /**< Number of text bytes. */
} InterfaceCatalogText;

/** @brief Ordered help-text sections for one tuning page. */
typedef struct {
    InterfaceCatalogText sections[INTERFACE_CATALOG_SECTION_COUNT]; /**< Ordered page sections. */
} InterfaceCatalogPage;

/** @brief Initializes a help-text macro with its byte length. */
#define HELP(value) {(value), (uint8_t)(sizeof(value) - 1u)}

/** @brief Initializes an empty help-text section. */
#define HELP_EMPTY {NULL, 0}

/** @brief Standard binary tuning catalog records. */
static const uint8_t normal_records[INTERFACE_CATALOG_RECORD_COUNT][INTERFACE_CATALOG_RECORD_SIZE] =
    {
        {0x01, 0x00, 0x09, 0x6c, 0x09, 0xfc, 0x09, 0xfc, 0xff, 0x10, 0x75,
         0xfc, 0x00, 0x00, 0x12, 0x7e, 0x53, 0x45, 0x4e, 0x01, 0x00},
        {0x02, 0x00, 0x00, 0x64, 0x01, 0xff, 0xff, 0x00, 0xff, 0x01, 0x4c,
         0x64, 0x00, 0x00, 0x14, 0x3c, 0x46, 0x46, 0x00, 0x01, 0x00},
        {0x03, 0x01, 0x01, 0x4f, 0x66, 0x66, 0x4f, 0x6e, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x53, 0x48, 0x4f, 0x01, 0x00},
        {0x04, 0x00, 0x00, 0x01, 0x01, 0xff, 0xff, 0x00, 0xff, 0x01, 0x01,
         0x64, 0x00, 0x00, 0x14, 0x32, 0x42, 0x4c, 0x49, 0x00, 0x00},
        {0x05, 0x01, 0x01, 0x4f, 0x66, 0x66, 0x4f, 0x6e, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x46, 0x53, 0x00, 0x00},
        {0x06, 0xfb, 0x03, 0x01, 0xff, 0xff, 0x00, 0xff, 0x01, 0x01, 0x03,
         0x00, 0x00, 0x01, 0x00, 0x44, 0x45, 0x41, 0x00, 0x00, 0x00},
        {0x07, 0xfb, 0x03, 0x01, 0xff, 0xff, 0x00, 0xff, 0x01, 0x01, 0x03,
         0x00, 0x00, 0x01, 0x00, 0x44, 0x52, 0x49, 0x00, 0x00, 0x00},
        {0x08, 0x00, 0x00, 0x78, 0x0a, 0xff, 0xff, 0x00, 0xff, 0x01, 0x6e,
         0x78, 0x00, 0x00, 0x14, 0x64, 0x46, 0x4f, 0x52, 0x00, 0x00},
        {0x09, 0x00, 0x00, 0x78, 0x0a, 0xff, 0xff, 0x00, 0xff, 0x01, 0x0a,
         0x64, 0x6e, 0x78, 0x14, 0x64, 0x53, 0x50, 0x52, 0x00, 0x00},
        {0x0a, 0x00, 0x00, 0x78, 0x0a, 0xff, 0xff, 0x00, 0xff, 0x01, 0x0a,
         0x64, 0x6e, 0x78, 0x14, 0x64, 0x44, 0x50, 0x52, 0x00, 0x00},
        {0x0b, 0x00, 0x00, 0x64, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x0e,
         0x32, 0x64, 0x00, 0x14, 0x32, 0x4e, 0x44, 0x50, 0x01, 0x00},
        {0x0c, 0x00, 0x00, 0x64, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x00,
         0x1e, 0x1f, 0x64, 0x14, 0x00, 0x4e, 0x46, 0x52, 0x00, 0x00},
        {0x0d, 0x00, 0x00, 0x64, 0x01, 0xff, 0xff, 0x00, 0x64, 0x02, 0x01,
         0x31, 0x33, 0x63, 0x14, 0x32, 0x42, 0x52, 0x46, 0x01, 0x00},
        {0x0e, 0x00, 0x00, 0x64, 0x01, 0xff, 0xff, 0x00, 0x64, 0x02, 0x01,
         0x31, 0x33, 0x63, 0x14, 0x32, 0x42, 0x52, 0x46, 0x01, 0x00},
        {0x0f, 0x00, 0x00, 0x64, 0x0a, 0xff, 0xff, 0xff, 0xff, 0xff, 0x14,
         0x5a, 0x64, 0x00, 0x14, 0x64, 0x46, 0x45, 0x49, 0x00, 0x00},
        {0x10, 0x01, 0x04, 0x45, 0x6e, 0x63, 0x43, 0x6f, 0x6e, 0x50, 0x75,
         0x6c, 0x41, 0x75, 0x74, 0x00, 0x4d, 0x50, 0x53, 0x01, 0x00},
        {0x11, 0x01, 0x04, 0x43, 0x42, 0x50, 0x43, 0x2f, 0x48, 0x42, 0x2f,
         0x54, 0x41, 0x6e, 0x61, 0x01, 0x41, 0x43, 0x50, 0x01, 0x00},
        {0x12, 0x00, 0x00, 0x14, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x01,
         0x08, 0x09, 0x14, 0x05, 0x06, 0x46, 0x4e, 0x54, 0x00, 0x00},
        {0x13, 0x00, 0x00, 0x64, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x00,
         0x14, 0x15, 0x64, 0x14, 0x00, 0x4e, 0x49, 0x4e, 0x00, 0x00},
        {0x14, 0x00, 0x00, 0x64, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x00,
         0x14, 0x15, 0x64, 0x14, 0x00, 0x4e, 0x49, 0x4e, 0x00, 0x00},
        {0x15, 0x00, 0x00, 0x64, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x00,
         0x14, 0x15, 0x64, 0x14, 0x00, 0x4e, 0x49, 0x4e, 0x00, 0x00},
        {0x16, 0x00, 0x00, 0x64, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x00,
         0x14, 0x15, 0x64, 0x14, 0x00, 0x4e, 0x49, 0x4e, 0x00, 0x00},
        {0x17, 0x00, 0x00, 0x05, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x02,
         0x03, 0x04, 0x00, 0x14, 0x03, 0x42, 0x50, 0x43, 0x01, 0x00},
        {0x18, 0x00, 0x00, 0x05, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x02,
         0x03, 0x04, 0x00, 0x14, 0x03, 0x43, 0x50, 0x43, 0x01, 0x00},
        {0x19, 0x00, 0x00, 0x05, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x02,
         0x03, 0x04, 0x00, 0x14, 0x03, 0x54, 0x50, 0x43, 0x01, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xf5, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0x00, 0x00},
};

/** @brief Legacy binary tuning catalog records. */
static const uint8_t legacy_records[INTERFACE_CATALOG_RECORD_COUNT][INTERFACE_CATALOG_RECORD_SIZE] =
    {
        {0x01, 0x00, 0x09, 0x6c, 0x09, 0xfc, 0x09, 0xfc, 0xff, 0x10, 0x00,
         0x7d, 0x7e, 0x00, 0x12, 0x7e, 0x53, 0x45, 0x4e, 0x01, 0x00},
        {0x02, 0x00, 0x00, 0x64, 0x01, 0xff, 0xff, 0x00, 0xff, 0x01, 0x00,
         0x4b, 0x63, 0x00, 0x14, 0x3c, 0x46, 0x46, 0x00, 0x01, 0x00},
        {0x03, 0x01, 0x01, 0x4f, 0x66, 0x66, 0x4f, 0x6e, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x53, 0x48, 0x4f, 0x01, 0x00},
        {0x04, 0x00, 0x00, 0x65, 0x01, 0xff, 0xff, 0x00, 0xff, 0x01, 0x64,
         0x00, 0x00, 0x00, 0x14, 0x32, 0x42, 0x4c, 0x49, 0x00, 0x00},
        {0x05, 0x01, 0x01, 0x4f, 0x66, 0x66, 0x4f, 0x6e, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x46, 0x53, 0x00, 0x00},
        {0x06, 0xfb, 0x03, 0x01, 0xff, 0xff, 0x00, 0xff, 0x01, 0x01, 0x03,
         0x00, 0x00, 0x01, 0x00, 0x44, 0x45, 0x41, 0x00, 0x00, 0x00},
        {0x07, 0xfb, 0x03, 0x01, 0xff, 0xff, 0x00, 0xff, 0x01, 0x01, 0x03,
         0x00, 0x00, 0x01, 0x00, 0x44, 0x52, 0x49, 0x00, 0x00, 0x00},
        {0x08, 0x00, 0x00, 0x78, 0x0a, 0xff, 0xff, 0x00, 0xff, 0x01, 0x00,
         0x0a, 0x0c, 0x00, 0x14, 0x64, 0x46, 0x4f, 0x52, 0x00, 0x00},
        {0x09, 0x00, 0x00, 0x78, 0x0a, 0xff, 0xff, 0x00, 0xff, 0x01, 0x00,
         0x0a, 0x0c, 0x00, 0x14, 0x64, 0x53, 0x50, 0x52, 0x00, 0x00},
        {0x0a, 0x00, 0x00, 0x78, 0x0a, 0xff, 0xff, 0x00, 0xff, 0x01, 0x00,
         0x0a, 0x0c, 0x00, 0x14, 0x64, 0x44, 0x50, 0x52, 0x00, 0x00},
        {0x0b, 0x00, 0x00, 0x64, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x0e,
         0x32, 0x63, 0x00, 0x14, 0x32, 0x4e, 0x44, 0x50, 0x01, 0x00},
        {0x0c, 0x00, 0x00, 0x64, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x1e,
         0x63, 0x00, 0x00, 0x14, 0x00, 0x4e, 0x46, 0x52, 0x00, 0x00},
        {0x0d, 0x00, 0x00, 0x64, 0x01, 0xff, 0xff, 0x00, 0x64, 0x02, 0x00,
         0x31, 0x32, 0x63, 0x14, 0x32, 0x42, 0x52, 0x46, 0x01, 0x00},
        {0x0e, 0x00, 0x00, 0x64, 0x01, 0xff, 0xff, 0x00, 0x64, 0x02, 0x00,
         0x31, 0x32, 0x63, 0x14, 0x32, 0x42, 0x52, 0x46, 0x01, 0x00},
        {0x0f, 0x00, 0x00, 0x64, 0x0a, 0xff, 0xff, 0xff, 0xff, 0xff, 0x14,
         0x5a, 0x63, 0x00, 0x14, 0x64, 0x46, 0x45, 0x49, 0x00, 0x00},
        {0x10, 0x01, 0x04, 0x45, 0x6e, 0x63, 0x43, 0x6f, 0x6e, 0x50, 0x75,
         0x6c, 0x41, 0x75, 0x74, 0x00, 0x4d, 0x50, 0x53, 0x01, 0x00},
        {0x11, 0x01, 0x04, 0x43, 0x42, 0x50, 0x43, 0x2f, 0x48, 0x42, 0x2f,
         0x54, 0x41, 0x6e, 0x61, 0x01, 0x41, 0x43, 0x50, 0x01, 0x00},
        {0x12, 0x00, 0x00, 0x14, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x00,
         0x08, 0x13, 0x00, 0x05, 0x06, 0x46, 0x4e, 0x54, 0x00, 0x00},
        {0x13, 0x00, 0x00, 0x64, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x14,
         0x63, 0x00, 0x00, 0x14, 0x00, 0x4e, 0x49, 0x4e, 0x00, 0x00},
        {0x14, 0x00, 0x00, 0x64, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x00,
         0x00, 0x00, 0x00, 0x14, 0x00, 0x46, 0x55, 0x4c, 0x01, 0x00},
        {0x15, 0x00, 0x00, 0x01, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x00,
         0x00, 0x00, 0x00, 0x14, 0x01, 0x42, 0x49, 0x4c, 0x01, 0x00},
        {0x16, 0x00, 0x00, 0x01, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x00,
         0x00, 0x00, 0x00, 0x14, 0x01, 0x44, 0x49, 0x52, 0x01, 0x00},
        {0x17, 0x00, 0x00, 0x05, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x02,
         0x03, 0x04, 0x00, 0x14, 0x03, 0x42, 0x50, 0x43, 0x01, 0x00},
        {0x18, 0x00, 0x00, 0x05, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x02,
         0x03, 0x04, 0x00, 0x14, 0x03, 0x43, 0x50, 0x43, 0x01, 0x00},
        {0x19, 0x00, 0x00, 0x05, 0x01, 0xff, 0xff, 0x00, 0xff, 0x02, 0x02,
         0x03, 0x04, 0x00, 0x14, 0x03, 0x47, 0x50, 0x43, 0x01, 0x00},
        {0x53, 0x45, 0x54, 0x55, 0x50, 0x00, 0x53, 0x65, 0x6c, 0x65, 0x63,
         0x74, 0x65, 0x64, 0x20, 0x54, 0x75, 0x6e, 0x69, 0x6e, 0x67},
};

/** @brief Protocol section code for each indexed-help section. */
static const uint8_t section_codes[INTERFACE_CATALOG_SECTION_COUNT] = {3, 0, 4, 11, 12, 13, 14};

/** @brief Indexed tuning-help pages in protocol order. */
static const InterfaceCatalogPage help_pages[INTERFACE_CATALOG_PAGE_COUNT] = {
    {.sections = {HELP("SETUP"), HELP("Selected Tuning Menu Setup"), HELP(" "), HELP(" "),
                  HELP(" "), HELP(" "), HELP(" ")}},
    {.sections = {HELP("Sensitivity"), HELP("Steering range in degrees"), HELP(" "),
                  HELP("High rotation values not supported by every game."),
                  HELP("Game is allowed to adjust SEN. Uses {Max A} if not supported."), HELP(" "),
                  HELP(" ")}},
    {.sections = {HELP("Force Feedback Str."), HELP("Overall maximum Force Feedback Strength"),
                  HELP("! FFB disabled."), HELP(" "), HELP("! High FFB can be dangerous!"),
                  HELP(" "), HELP(" ")}},
    {.sections = {HELP("Vibration"), HELP("Vibration Motor strength"),
                  HELP("Disables rumble motors of the steering wheel."), HELP(" "), HELP(" "),
                  HELP(" "), HELP(" ")}},
    {.sections = {HELP("Brake Level Indicator"),
                  HELP("Required brake input % to trigger vibration"),
                  HELP("Amount of brake input needed to trigger rumble."), HELP(" "), HELP(" "),
                  HELP(" "), HELP("Only the game will trigger brake pedal vibration.")}},
    {.sections = {HELP("FF Scale"), HELP("Force feedback scaling mode"),
                  HELP("Maintains consistent and reliable torque output."), HELP(" "), HELP(" "),
                  HELP(" "), HELP("Allows maximum peak torque output.")}},
    {.sections = {HELP("DEA"), HELP("Wheel axis damping or acceleration"),
                  HELP("Reduce to dampen wheel axis/oscillations and raise to speed up"),
                  HELP("High DRI can increase oscillations."), HELP(" "), HELP(" "), HELP(" ")}},
    {.sections = {HELP("Drift Mode"), HELP("Wheel axis damping or acceleration"),
                  HELP("Reduce to dampen wheel axis/oscillations and raise to speed up"),
                  HELP("High DRI can increase oscillations."), HELP(" "), HELP(" "), HELP(" ")}},
    {.sections = {HELP("Force Effect Str."), HELP("Force Effect Strength modifier"),
                  HELP("Force effects disabled."), HELP(" "), HELP("! Will cause clipping!"),
                  HELP(" "), HELP(" ")}},
    {.sections = {HELP("Spring Effect Str."), HELP("Spring Effect Strength modifier"),
                  HELP("Spring effects disabled."),
                  HELP("Only adjusts game effects. No constant effect applied."),
                  HELP("! Will cause clipping!"), HELP(" "), HELP(" ")}},
    {.sections = {HELP("Damper Effect Str."), HELP("Damper Effect Strength modifier"),
                  HELP("Damper effects disabled."),
                  HELP("Only adjusts game effects. No constant effect applied."),
                  HELP("! Will cause clipping!"), HELP(" "), HELP(" ")}},
    {.sections = {HELP("Natural Damper"), HELP("Mechanical wheel axis damping simulation"),
                  HELP("Low NDP can be dangerous!"),
                  HELP("NDP can add realistic feeling and prevent oscillation."),
                  HELP("High NDP slows down reaction and reduces FFB detail."), HELP(" "),
                  HELP(" ")}},
    {.sections = {HELP("Natural Friction"), HELP("Mechanical wheel axis friction simulation"),
                  HELP("NFR can add realistic feeling and prevent oscillation."),
                  HELP("High NFR slows down reaction and reduces FFB detail."), HELP(" "),
                  HELP(" "), HELP(" ")}},
    {.sections = {HELP("Brake Force"), HELP("Adjust load cell sensitivity"),
                  HELP("Minimum force needed."), HELP("Less braking force needed."), HELP(" "),
                  HELP("More braking force needed."), HELP("Maximum force needed.")}},
    {.sections = {HELP("Brake Force"), HELP("Adjust load cell sensitivity"),
                  HELP("Minimum force needed."), HELP("Less braking force needed."), HELP(" "),
                  HELP("More braking force needed."), HELP("Maximum force needed.")}},
    {.sections = {HELP("FE Intensitiy"), HELP("Force Effect spikes intensity modifier"),
                  HELP("Low FEI reduces FFB detail."), HELP("Lower settings smoothen FFB."),
                  HELP("1:1 FFB from game."), HELP(" "), HELP(" ")}},
    {.sections = {HELP("MPS Mode"), HELP("Multi-Position Switch mode selector"), HELP(" "),
                  HELP(" "), HELP(" "), HELP(" "), HELP_EMPTY}},
    {.sections = {HELP("ACP Mode"), HELP("Analogue Clutch Paddle mode selector"), HELP(" "),
                  HELP(" "), HELP(" "), HELP(" "), HELP_EMPTY}},
    {.sections = {HELP("FFB Interpolation"), HELP("FFB Interpolation Filter level"),
                  HELP("Unfiltered, raw FFB signal. Can feel noisy and rough."),
                  HELP("Interpolation enabled. Fast, smooth FFB signal."),
                  HELP("High interpolation can reduce detail and response."), HELP(" "),
                  HELP(" ")}},
    {.sections = {HELP("Natural Inertia"), HELP("Mechanical wheel axis inertia simulation"),
                  HELP("Adds weighted feel and can reduce initial oscillation."),
                  HELP("High NIN slows down reaction and can increase oscillation."), HELP(" "),
                  HELP(" "), HELP(" ")}},
    {.sections = {HELP("FUL Mode"), HELP("Analogue Clutch Paddle mode selector"), HELP(" "),
                  HELP(" "), HELP(" "), HELP(" "), HELP_EMPTY}},
    {.sections = {HELP("BIL Mode"), HELP("Analogue Clutch Paddle mode selector"), HELP(" "),
                  HELP(" "), HELP(" "), HELP(" "), HELP_EMPTY}},
    {.sections = {HELP("ROT Mode"), HELP("Analogue Clutch Paddle mode selector"), HELP(" "),
                  HELP(" "), HELP(" "), HELP(" "), HELP_EMPTY}},
    {.sections = {HELP("FullForce Effect Str."), HELP("FullForce Effect Strength modifier"),
                  HELP(" "), HELP(" "), HELP(" "), HELP(" "), HELP(" ")}},
    {.sections = {HELP("Button Illumination"), HELP("Toggle button illumination"),
                  HELP("Button illumination disabled."), HELP(" "), HELP(" "), HELP(" "),
                  HELP(" ")}},
    {.sections = {HELP("Display Rotation"), HELP("Toggle display rotation"),
                  HELP("Display rotation disabled."), HELP(" "), HELP(" "), HELP(" "), HELP(" ")}},
};

#undef HELP_EMPTY
#undef HELP

/**
 * @brief Initializes host-interface catalog transfer state.
 *
 * Clears both pending streams and resets their record, page, section, and chunk positions.
 *
 * @param[out] catalog Catalog transfer state to initialize.
 */
void wheel_interface_catalog_init(WheelInterfaceCatalog *catalog) {
    if (catalog != NULL) {
        *catalog = (WheelInterfaceCatalog){0};
    }
}

/**
 * @brief Starts one host-interface catalog presentation.
 *
 * Mode four marks the remote-tuning record stream pending. Mode five marks indexed help pending.
 * Each mode resets only an already exhausted stream; activation preserves midstream cursors and
 * leaves the other stream pending. Other values leave both independent streams unchanged.
 *
 * @param[in,out] catalog Catalog transfer state.
 * @param[in] mode Requested host-interface presentation mode.
 * @return True when a catalog stream was marked pending.
 */
bool wheel_interface_catalog_activate(WheelInterfaceCatalog *catalog, uint8_t mode) {
    if (catalog == NULL) {
        return false;
    }
    if (mode == 4) {
        if (catalog->record_index >= INTERFACE_CATALOG_RECORD_COUNT) {
            catalog->record_index = 0;
        }
        catalog->records_pending = true;
        return true;
    }
    if (mode == 5) {
        if (catalog->page_index >= INTERFACE_CATALOG_PAGE_COUNT) {
            catalog->page_index = 0;
            catalog->section_index = 0;
            catalog->chunk_index = 0;
        }
        catalog->configuration_pending = true;
        return true;
    }
    return false;
}

/**
 * @brief Encodes the next remote-tuning catalog record.
 *
 * Emits packet type 0x80 followed by one binary-backed 21-byte definition. Extended remote-tuning
 * wheels use the legacy catalog and skip its unsupported force-scale record.
 *
 * @param[in,out] catalog Catalog transfer state.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[out] frame Thirty-three-byte attached-wheel response frame.
 */
static void encode_record(WheelInterfaceCatalog *catalog, uint8_t wheel_mode, uint8_t frame[33]) {
    const uint8_t (*records)[INTERFACE_CATALOG_RECORD_SIZE] =
        wheel_mode == INTERFACE_CATALOG_EXTENDED_WHEEL_MODE ? legacy_records : normal_records;
    if (wheel_mode == INTERFACE_CATALOG_EXTENDED_WHEEL_MODE && catalog->record_index == 4) {
        catalog->record_index = 5;
    }

    memset(frame + 1, 0, 31);
    frame[1] = INTERFACE_CATALOG_REMOTE_PACKET_TYPE;
    memcpy(frame + 2, records[catalog->record_index], INTERFACE_CATALOG_RECORD_SIZE);
    catalog->record_index++;
    if (catalog->record_index >= INTERFACE_CATALOG_RECORD_COUNT) {
        catalog->records_pending = false;
    }
}

/**
 * @brief Advances the indexed tuning-help stream to its next section.
 *
 * Resets the chunk position, wraps sections into the next page, and closes the stream after the
 * final page.
 *
 * @param[in,out] catalog Catalog transfer state.
 */
static void advance_configuration_section(WheelInterfaceCatalog *catalog) {
    catalog->chunk_index = 0;
    catalog->section_index++;
    if (catalog->section_index >= INTERFACE_CATALOG_SECTION_COUNT) {
        catalog->section_index = 0;
        catalog->page_index++;
        if (catalog->page_index >= INTERFACE_CATALOG_PAGE_COUNT) {
            catalog->configuration_pending = false;
        }
    }
}

/**
 * @brief Encodes the next indexed tuning-help text frame.
 *
 * Emits every section, including zero-length sections, and advances each section as its chunks
 * are emitted.
 *
 * @param[in,out] catalog Catalog transfer state.
 * @param[out] frame Thirty-three-byte attached-wheel response frame.
 * @return True when a configuration frame was encoded.
 */
static bool encode_configuration(WheelInterfaceCatalog *catalog, uint8_t frame[33]) {
    const InterfaceCatalogText *text =
        &help_pages[catalog->page_index].sections[catalog->section_index];

    uint8_t full_chunks = text->length > INTERFACE_CATALOG_CHUNK_SIZE
                              ? (uint8_t)(text->length / INTERFACE_CATALOG_CHUNK_SIZE)
                              : 0;
    uint8_t remaining = (uint8_t)(full_chunks - catalog->chunk_index);
    uint16_t offset = (uint16_t)catalog->chunk_index * INTERFACE_CATALOG_CHUNK_SIZE;

    memset(frame, 0, 32);
    frame[0] = INTERFACE_CATALOG_CONFIGURATION_REPORT_ID;
    frame[1] = INTERFACE_CATALOG_CONFIGURATION_PACKET_TYPE;
    frame[2] = catalog->page_index;
    frame[3] = section_codes[catalog->section_index];
    frame[4] = remaining;

    if (remaining != 0) {
        memcpy(frame + 5, text->text + offset, INTERFACE_CATALOG_CHUNK_SIZE);
        catalog->chunk_index++;
        return true;
    }
    if (text->length > offset) {
        memcpy(frame + 5, text->text + offset, text->length - offset);
    }
    advance_configuration_section(catalog);
    return true;
}

/**
 * @brief Encodes the next pending host-interface catalog frame.
 *
 * Emits catalog frames only in legacy wheel mode 0x0e. Gives the remote-tuning record stream
 * priority over indexed help and consumes one transfer step.
 *
 * @param[in,out] catalog Catalog transfer state.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[out] frame Thirty-three-byte attached-wheel response frame.
 * @return True when a catalog frame was encoded.
 */
bool wheel_interface_catalog_encode_next(WheelInterfaceCatalog *catalog, uint8_t wheel_mode,
                                         uint8_t frame[33]) {
    if (catalog == NULL || frame == NULL) {
        return false;
    }
    if (wheel_mode != INTERFACE_CATALOG_LEGACY_WHEEL_MODE) {
        return false;
    }
    if (catalog->records_pending) {
        encode_record(catalog, wheel_mode, frame);
        return true;
    }
    if (catalog->configuration_pending) {
        return encode_configuration(catalog, frame);
    }
    return false;
}
