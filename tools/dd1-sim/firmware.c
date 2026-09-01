#define WIN32_LEAN_AND_MEAN
#include "firmware.h"

#include <dspic33.h>
#include <kinetis.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wincrypt.h>

enum {
    SECRET_SIZE = 16,
    AES_BLOCK_SIZE = 16,
    METADATA_ADDRESS = 0xa3c0,
    METADATA_SIZE_ADDRESS = 0xa3c8,
};

typedef struct {
    uint8_t *data;
    size_t size;
} ByteBuffer;

typedef bool (*DataRecord)(void *context, uint32_t address, const uint8_t *data, size_t size);

typedef enum {
    FIRMWARE_UNKNOWN,
    FIRMWARE_BASE,
    FIRMWARE_WQR,
    FIRMWARE_MOTOR,
} FirmwareKind;

typedef struct {
    uint8_t metadata[20];
    bool metadata_present[20];
} FirmwareProbe;

static void write_error(char *error, size_t error_size, const char *format, ...) {
    if (error == NULL || error_size == 0) {
        return;
    }
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(error, error_size, format, arguments);
    va_end(arguments);
}

static bool read_file(const char *path, ByteBuffer *buffer) {
    if (path == NULL || buffer == NULL) {
        return false;
    }
    FILE *file = fopen(path, "rb");
    long length;
    if (file == NULL || fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) <= 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        if (file != NULL) {
            fclose(file);
        }
        return false;
    }
    buffer->data = malloc((size_t)length);
    if (buffer->data == NULL || fread(buffer->data, 1, (size_t)length, file) != (size_t)length) {
        free(buffer->data);
        buffer->data = NULL;
        fclose(file);
        return false;
    }
    fclose(file);
    buffer->size = (size_t)length;
    return true;
}

static int hex_digit(int value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

static bool hex_byte(const char *text, uint8_t *value) {
    int high = hex_digit((unsigned char)text[0]);
    int low = hex_digit((unsigned char)text[1]);
    if (high < 0 || low < 0) {
        return false;
    }
    *value = (uint8_t)(high << 4 | low);
    return true;
}

static bool secret_from_text(const char *text, uint8_t secret[SECRET_SIZE]) {
    if (text == NULL || strlen(text) != SECRET_SIZE * 2) {
        return false;
    }
    for (size_t index = 0; index < SECRET_SIZE; ++index) {
        if (!hex_byte(text + index * 2, &secret[index])) {
            return false;
        }
    }
    return true;
}

static FILE *open_env_file(void) {
    FILE *file = fopen(".env", "r");
    if (file != NULL) {
        return file;
    }
    char path[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, path, sizeof(path));
    if (length == 0 || length >= sizeof(path)) {
        return NULL;
    }
    char *separator = strrchr(path, '\\');
    while (separator != NULL) {
        separator[1] = 0;
        if (strlen(path) + 4 < sizeof(path)) {
            strcat(path, ".env");
            file = fopen(path, "r");
            if (file != NULL) {
                return file;
            }
        }
        separator[0] = 0;
        separator = strrchr(path, '\\');
    }
    return NULL;
}

static bool secret_from_env_file(uint8_t secret[SECRET_SIZE]) {
    FILE *file = open_env_file();
    char line[256];
    const char prefix[] = "FANATEC_FIRMWARE_SECRET_HEX=";
    if (file == NULL) {
        return false;
    }
    bool found = false;
    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length = strcspn(line, "\r\n");
        line[length] = 0;
        if (strncmp(line, prefix, sizeof(prefix) - 1) == 0) {
            found = secret_from_text(line + sizeof(prefix) - 1, secret);
            break;
        }
    }
    fclose(file);
    return found;
}

static bool firmware_secret(bool legacy_wide, uint8_t output[SECRET_SIZE], char *error,
                            size_t error_size) {
    char text[SECRET_SIZE * 2 + 1];
    uint8_t secret[SECRET_SIZE];
    DWORD length = GetEnvironmentVariableA("FANATEC_FIRMWARE_SECRET_HEX", text, sizeof(text));
    bool available = length == SECRET_SIZE * 2 && secret_from_text(text, secret);
    if (!available) {
        available = secret_from_env_file(secret);
    }
    if (!available) {
        write_error(
            error, error_size,
            "set FANATEC_FIRMWARE_SECRET_HEX or provide it in .env beside or above the executable");
        return false;
    }
    if (!legacy_wide) {
        memcpy(output, secret, SECRET_SIZE);
        return true;
    }
    for (size_t index = 0; index < SECRET_SIZE / 2; ++index) {
        if (secret[index] < 0x20 || secret[index] > 0x7e) {
            write_error(error, error_size, "the legacy firmware secret is not ASCII");
            return false;
        }
        output[index * 2] = secret[index];
        output[index * 2 + 1] = 0;
    }
    return true;
}

static bool decrypt_file(const char *path, bool legacy_wide, ByteBuffer *plaintext, char *error,
                         size_t error_size) {
    ByteBuffer encrypted = {0};
    uint8_t secret[SECRET_SIZE];
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    HCRYPTKEY key = 0;
    bool decrypted = false;
    if (path == NULL || plaintext == NULL) {
        write_error(error, error_size, "firmware path is required");
        return false;
    }
    if (!firmware_secret(legacy_wide, secret, error, error_size) || !read_file(path, &encrypted)) {
        if (encrypted.data == NULL && error != NULL && error[0] == 0) {
            write_error(error, error_size, "could not read encrypted firmware %s", path);
        }
        return false;
    }
    if (encrypted.size == 0 || encrypted.size % AES_BLOCK_SIZE != 0 ||
        encrypted.size > UINT32_MAX) {
        write_error(error, error_size, "encrypted firmware has an invalid size");
        goto done;
    }
    DWORD size = (DWORD)encrypted.size;
    if (!CryptAcquireContextA(&provider, "",
                              "Microsoft Enhanced RSA and AES Cryptographic Provider", PROV_RSA_AES,
                              0) ||
        !CryptCreateHash(provider, CALG_MD5, 0, 0, &hash) ||
        !CryptHashData(hash, secret, SECRET_SIZE, 0) ||
        !CryptDeriveKey(provider, CALG_AES_128, hash, 128U << 16U, &key) ||
        !CryptDecrypt(key, 0, TRUE, 0, encrypted.data, &size)) {
        write_error(error, error_size, "could not decrypt firmware %s (Windows error %lu)", path,
                    (unsigned long)GetLastError());
        goto done;
    }
    plaintext->data = encrypted.data;
    plaintext->size = size;
    encrypted.data = NULL;
    decrypted = true;

done:
    if (key != 0) {
        CryptDestroyKey(key);
    }
    if (hash != 0) {
        CryptDestroyHash(hash);
    }
    if (provider != 0) {
        CryptReleaseContext(provider, 0);
    }
    free(encrypted.data);
    return decrypted;
}

static bool parse_ihex(const ByteBuffer *plaintext, DataRecord record, void *context, char *error,
                       size_t error_size) {
    if (plaintext == NULL || plaintext->data == NULL || plaintext->size == 0) {
        write_error(error, error_size, "decrypted firmware is empty");
        return false;
    }
    const char *cursor = memchr(plaintext->data, ':', plaintext->size);
    const char *end = (const char *)plaintext->data + plaintext->size;
    uint32_t upper_address = 0;
    bool data_seen = false;
    bool end_seen = false;
    if (cursor == NULL) {
        write_error(error, error_size, "decrypted firmware does not contain Intel HEX");
        return false;
    }
    while (cursor < end && *cursor == ':') {
        const char *line_end = cursor;
        while (line_end < end && *line_end != '\r' && *line_end != '\n') {
            ++line_end;
        }
        uint8_t header[4];
        if (line_end - cursor < 11 || !hex_byte(cursor + 1, &header[0]) ||
            !hex_byte(cursor + 3, &header[1]) || !hex_byte(cursor + 5, &header[2]) ||
            !hex_byte(cursor + 7, &header[3])) {
            write_error(error, error_size, "decrypted firmware has an invalid Intel HEX record");
            return false;
        }
        size_t byte_count = header[0];
        size_t record_size = byte_count + 5;
        if ((size_t)(line_end - cursor) != 1 + record_size * 2) {
            write_error(error, error_size,
                        "decrypted firmware has an invalid Intel HEX record length");
            return false;
        }
        uint8_t bytes[260];
        uint8_t sum = 0;
        for (size_t index = 0; index < record_size; ++index) {
            if (!hex_byte(cursor + 1 + index * 2, &bytes[index])) {
                write_error(error, error_size, "decrypted firmware has non-hexadecimal data");
                return false;
            }
            sum = (uint8_t)(sum + bytes[index]);
        }
        if (sum != 0) {
            write_error(error, error_size, "decrypted firmware has an invalid Intel HEX checksum");
            return false;
        }
        uint16_t address = (uint16_t)((uint16_t)bytes[1] << 8U | bytes[2]);
        uint8_t type = bytes[3];
        const uint8_t *data = bytes + 4;
        if (type == 0) {
            if (upper_address > UINT32_MAX - address ||
                byte_count > UINT32_MAX - (upper_address + address)) {
                write_error(error, error_size, "firmware address range overflows");
                return false;
            }
            uint32_t absolute = upper_address + address;
            if (record != NULL && !record(context, absolute, data, byte_count)) {
                write_error(error, error_size, "firmware data at 0x%08lx cannot be loaded",
                            (unsigned long)absolute);
                return false;
            }
            data_seen |= byte_count != 0;
        } else if (type == 1) {
            if (byte_count != 0 || address != 0) {
                write_error(error, error_size, "decrypted firmware has an invalid EOF record");
                return false;
            }
            end_seen = true;
        } else if (type == 2 && byte_count == 2 && address == 0) {
            upper_address = (uint32_t)((uint16_t)data[0] << 8U | data[1]) << 4U;
        } else if (type == 4 && byte_count == 2 && address == 0) {
            upper_address = (uint32_t)((uint16_t)data[0] << 8U | data[1]) << 16U;
        } else if ((type == 3 || type == 5) && byte_count == 4 && address == 0) {
        } else {
            write_error(error, error_size, "decrypted firmware uses unsupported Intel HEX type %u",
                        type);
            return false;
        }
        cursor = line_end;
        while (cursor < end && (*cursor == '\r' || *cursor == '\n')) {
            ++cursor;
        }
        if (end_seen) {
            while (cursor < end &&
                   (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')) {
                ++cursor;
            }
            if (cursor != end) {
                write_error(error, error_size, "decrypted firmware has data after its EOF record");
                return false;
            }
            break;
        }
        if (cursor < end && *cursor != ':') {
            write_error(error, error_size, "decrypted firmware has trailing non-record data");
            return false;
        }
    }
    if (!data_seen || !end_seen) {
        write_error(error, error_size, "decrypted firmware is incomplete");
        return false;
    }
    return true;
}

static bool load_base_record(void *context, uint32_t address, const uint8_t *data, size_t size) {
    Dspic33 *device = context;
    if ((address & 3U) != 0 || (size & 3U) != 0) {
        return false;
    }
    for (size_t offset = 0; offset < size; offset += 4) {
        uint32_t word = (uint32_t)data[offset] | (uint32_t)data[offset + 1] << 8U |
                        (uint32_t)data[offset + 2] << 16U;
        uint32_t program_address = (address + (uint32_t)offset) / 2U;
        if (word == UINT32_C(0x00ffffff)) {
            continue;
        }
        bool configuration =
            program_address >= DSPIC33_CONFIGURATION_BASE &&
            program_address < DSPIC33_CONFIGURATION_BASE + DSPIC33_CONFIGURATION_SIZE;
        if (!(configuration ? dspic33_load_configuration_word(device, program_address, word)
                            : dspic33_load_program_word(device, program_address, word))) {
            return false;
        }
    }
    return true;
}

static bool load_kinetis_record(void *context, uint32_t address, const uint8_t *data, size_t size) {
    return size == 0 || kinetis_load(context, address, data, size);
}

bool dd1_firmware_load_base(Dspic33 *device, const char *path, char *error, size_t error_size) {
    if (error != NULL && error_size != 0) {
        error[0] = 0;
    }
    if (device == NULL || path == NULL) {
        write_error(error, error_size, "base device and firmware path are required");
        return false;
    }
    ByteBuffer plaintext = {0};
    bool loaded = decrypt_file(path, false, &plaintext, error, error_size) &&
                  parse_ihex(&plaintext, load_base_record, device, error, error_size);
    free(plaintext.data);
    return loaded;
}

bool dd1_firmware_load_kinetis(Kinetis *device, const char *path, bool legacy_wide_secret,
                               char *error, size_t error_size) {
    if (error != NULL && error_size != 0) {
        error[0] = 0;
    }
    if (device == NULL || path == NULL) {
        write_error(error, error_size, "Kinetis device and firmware path are required");
        return false;
    }
    ByteBuffer plaintext = {0};
    bool loaded = decrypt_file(path, legacy_wide_secret, &plaintext, error, error_size) &&
                  parse_ihex(&plaintext, load_kinetis_record, device, error, error_size);
    free(plaintext.data);
    return loaded;
}

static bool probe_record(void *context, uint32_t address, const uint8_t *data, size_t size) {
    FirmwareProbe *probe = context;
    for (size_t index = 0; index < size; ++index) {
        uint32_t current = address + (uint32_t)index;
        if (current >= METADATA_ADDRESS && current < METADATA_ADDRESS + sizeof(probe->metadata)) {
            size_t destination = current - METADATA_ADDRESS;
            probe->metadata[destination] = data[index];
            probe->metadata_present[destination] = true;
        }
    }
    return true;
}

static FirmwareKind identify_plaintext(const ByteBuffer *plaintext, uint32_t *score) {
    const uint8_t *first_record = memchr(plaintext->data, ':', plaintext->size);
    if (first_record != NULL && (size_t)(first_record - plaintext->data) >= 6 &&
        memcmp(plaintext->data, "0x0706", 6) == 0) {
        *score = (uint32_t)plaintext->size;
        return FIRMWARE_BASE;
    }
    FirmwareProbe probe = {0};
    if (!parse_ihex(plaintext, probe_record, &probe, NULL, 0)) {
        return FIRMWARE_UNKNOWN;
    }
    for (size_t index = 0; index < sizeof(probe.metadata); ++index) {
        if (!probe.metadata_present[index]) {
            return FIRMWARE_UNKNOWN;
        }
    }
    *score = (uint32_t)probe.metadata[16] | (uint32_t)probe.metadata[17] << 8U |
             (uint32_t)probe.metadata[18] << 16U | (uint32_t)probe.metadata[19] << 24U;
    if (memcmp(probe.metadata, "wqrb", 4) == 0) {
        return FIRMWARE_WQR;
    }
    if (memcmp(probe.metadata, "dd10", 4) == 0) {
        return FIRMWARE_MOTOR;
    }
    return FIRMWARE_UNKNOWN;
}

static bool probe_file(const char *path, FirmwareKind *kind, uint32_t *score) {
    ByteBuffer plaintext = {0};
    char ignored[1] = {0};
    if (decrypt_file(path, false, &plaintext, ignored, 0)) {
        *kind = identify_plaintext(&plaintext, score);
        free(plaintext.data);
        if (*kind != FIRMWARE_UNKNOWN) {
            return true;
        }
    }
    plaintext = (ByteBuffer){0};
    if (decrypt_file(path, true, &plaintext, ignored, 0)) {
        *kind = identify_plaintext(&plaintext, score);
        free(plaintext.data);
        return *kind != FIRMWARE_UNKNOWN;
    }
    return false;
}

static void retain_candidate(char destination[MAX_PATH], uint32_t *current_score, const char *path,
                             uint32_t score) {
    if (destination[0] == 0 || score > *current_score) {
        snprintf(destination, MAX_PATH, "%s", path);
        *current_score = score;
    }
}

bool dd1_firmware_find(const char *directory, Dd1FirmwarePaths *paths, char *error,
                       size_t error_size) {
    if (error != NULL && error_size != 0) {
        error[0] = 0;
    }
    if (directory == NULL || paths == NULL) {
        write_error(error, error_size, "firmware directory and output paths are required");
        return false;
    }
    char pattern[MAX_PATH];
    WIN32_FIND_DATAA entry;
    uint32_t scores[4] = {0};
    memset(paths, 0, sizeof(*paths));
    int length = snprintf(pattern, sizeof(pattern), "%s\\*.hex", directory);
    if (length <= 0 || length >= (int)sizeof(pattern)) {
        write_error(error, error_size, "firmware directory path is too long");
        return false;
    }
    HANDLE search = FindFirstFileA(pattern, &entry);
    if (search == INVALID_HANDLE_VALUE) {
        write_error(error, error_size, "could not scan firmware directory %s", directory);
        return false;
    }
    do {
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        char path[MAX_PATH];
        if (snprintf(path, sizeof(path), "%s\\%s", directory, entry.cFileName) >=
            (int)sizeof(path)) {
            continue;
        }
        FirmwareKind kind;
        uint32_t score;
        if (!probe_file(path, &kind, &score)) {
            continue;
        }
        if (kind == FIRMWARE_BASE) {
            retain_candidate(paths->base, &scores[kind], path, score);
        } else if (kind == FIRMWARE_WQR) {
            retain_candidate(paths->wqr, &scores[kind], path, score);
        } else if (kind == FIRMWARE_MOTOR) {
            retain_candidate(paths->motor, &scores[kind], path, score);
        }
    } while (FindNextFileA(search, &entry));
    FindClose(search);
    if (paths->base[0] == 0 || paths->wqr[0] == 0 || paths->motor[0] == 0) {
        write_error(error, error_size,
                    "could not identify DD1 base, WQR, and motor encrypted HEX files in %s",
                    directory);
        return false;
    }
    return true;
}
