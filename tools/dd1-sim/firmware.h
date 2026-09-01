#ifndef OPENTEC_DD1_SIM_FIRMWARE_H
#define OPENTEC_DD1_SIM_FIRMWARE_H

#include <stdbool.h>
#include <stddef.h>
#include <windows.h>

typedef struct Dspic33 Dspic33;
typedef struct Kinetis Kinetis;

typedef struct {
    char base[MAX_PATH];
    char wqr[MAX_PATH];
    char motor[MAX_PATH];
} Dd1FirmwarePaths;

bool dd1_firmware_find(const char *directory, Dd1FirmwarePaths *paths, char *error,
                       size_t error_size);
bool dd1_firmware_load_base(Dspic33 *device, const char *path, char *error, size_t error_size);
bool dd1_firmware_load_kinetis(Kinetis *device, const char *path, bool legacy_wide_secret,
                               char *error, size_t error_size);

#endif
