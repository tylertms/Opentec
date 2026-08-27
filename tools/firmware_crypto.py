import argparse
import ctypes
import os
import sys
import tempfile
from ctypes import wintypes
from pathlib import Path

PROVIDER_TYPE_RSA_AES = 24
ALGORITHM_MD5 = 0x00008003
ALGORITHM_AES_128 = 0x0000660E
AES_128_KEY_FLAGS = 128 << 16
AES_BLOCK_SIZE = 16
MAX_DWORD = (1 << 32) - 1
PROVIDER_NAME = b"Microsoft Enhanced RSA and AES Cryptographic Provider"
SECRET_NAME = "FANATEC_FIRMWARE_SECRET_HEX"
ENV_FILE = Path(__file__).resolve().parent.parent / ".env"


class FirmwareCipher:
    def __init__(self, secret):
        if sys.platform != "win32":
            raise RuntimeError("Fanatec firmware encryption requires Windows CryptoAPI")

        self.api = ctypes.WinDLL("advapi32", use_last_error=True)
        self.provider = ctypes.c_size_t()
        self.key = ctypes.c_size_t()
        self._configure_api()
        try:
            self._create_key(secret)
        except Exception:
            self.close()
            raise

    def __enter__(self):
        return self

    def __exit__(self, exception_type, exception, traceback):
        self.close()

    def close(self):
        if self.key.value:
            self.api.CryptDestroyKey(self.key)
            self.key = ctypes.c_size_t()

        if self.provider.value:
            self.api.CryptReleaseContext(self.provider, 0)
            self.provider = ctypes.c_size_t()

    def encrypt(self, payload):
        if len(payload) > MAX_DWORD - AES_BLOCK_SIZE:
            raise ValueError("Firmware payload is too large for Windows CryptoAPI")

        capacity = len(payload) + AES_BLOCK_SIZE
        buffer = (ctypes.c_ubyte * capacity)()
        buffer[: len(payload)] = payload
        encrypted_size = wintypes.DWORD(len(payload))
        self._check(
            self.api.CryptEncrypt(
                self.key,
                ctypes.c_size_t(),
                True,
                0,
                buffer,
                ctypes.byref(encrypted_size),
                capacity,
            ),
            "Firmware encryption failed",
        )
        return bytes(buffer[: encrypted_size.value])

    def decrypt(self, payload):
        if not payload or len(payload) % AES_BLOCK_SIZE:
            raise ValueError(
                f"Encrypted firmware size must be a nonzero multiple of {AES_BLOCK_SIZE}"
            )

        buffer = (ctypes.c_ubyte * len(payload)).from_buffer_copy(payload)
        decrypted_size = wintypes.DWORD(len(payload))
        self._check(
            self.api.CryptDecrypt(
                self.key,
                ctypes.c_size_t(),
                True,
                0,
                buffer,
                ctypes.byref(decrypted_size),
            ),
            "Firmware decryption failed",
        )
        return bytes(buffer[: decrypted_size.value])

    def _configure_api(self):
        handle = ctypes.c_size_t
        byte_pointer = ctypes.POINTER(ctypes.c_ubyte)

        self.api.CryptAcquireContextA.argtypes = [
            ctypes.POINTER(handle),
            ctypes.c_char_p,
            ctypes.c_char_p,
            wintypes.DWORD,
            wintypes.DWORD,
        ]
        self.api.CryptAcquireContextA.restype = wintypes.BOOL
        self.api.CryptCreateHash.argtypes = [
            handle,
            wintypes.DWORD,
            handle,
            wintypes.DWORD,
            ctypes.POINTER(handle),
        ]
        self.api.CryptCreateHash.restype = wintypes.BOOL
        self.api.CryptHashData.argtypes = [
            handle,
            byte_pointer,
            wintypes.DWORD,
            wintypes.DWORD,
        ]
        self.api.CryptHashData.restype = wintypes.BOOL
        self.api.CryptDeriveKey.argtypes = [
            handle,
            wintypes.DWORD,
            handle,
            wintypes.DWORD,
            ctypes.POINTER(handle),
        ]
        self.api.CryptDeriveKey.restype = wintypes.BOOL
        self.api.CryptEncrypt.argtypes = [
            handle,
            handle,
            wintypes.BOOL,
            wintypes.DWORD,
            byte_pointer,
            ctypes.POINTER(wintypes.DWORD),
            wintypes.DWORD,
        ]
        self.api.CryptEncrypt.restype = wintypes.BOOL
        self.api.CryptDecrypt.argtypes = [
            handle,
            handle,
            wintypes.BOOL,
            wintypes.DWORD,
            byte_pointer,
            ctypes.POINTER(wintypes.DWORD),
        ]
        self.api.CryptDecrypt.restype = wintypes.BOOL
        self.api.CryptDestroyKey.argtypes = [handle]
        self.api.CryptDestroyKey.restype = wintypes.BOOL
        self.api.CryptDestroyHash.argtypes = [handle]
        self.api.CryptDestroyHash.restype = wintypes.BOOL
        self.api.CryptReleaseContext.argtypes = [handle, wintypes.DWORD]
        self.api.CryptReleaseContext.restype = wintypes.BOOL

    def _create_key(self, secret):
        self._check(
            self.api.CryptAcquireContextA(
                ctypes.byref(self.provider),
                b"",
                PROVIDER_NAME,
                PROVIDER_TYPE_RSA_AES,
                0,
            ),
            "The tool failed to acquire the cryptographic provider",
        )

        digest = ctypes.c_size_t()
        try:
            self._check(
                self.api.CryptCreateHash(
                    self.provider,
                    ALGORITHM_MD5,
                    ctypes.c_size_t(),
                    0,
                    ctypes.byref(digest),
                ),
                "The tool failed to create the firmware-key digest",
            )
            secret_buffer = (ctypes.c_ubyte * len(secret)).from_buffer_copy(secret)
            self._check(
                self.api.CryptHashData(digest, secret_buffer, len(secret), 0),
                "The tool failed to hash the firmware secret",
            )
            self._check(
                self.api.CryptDeriveKey(
                    self.provider,
                    ALGORITHM_AES_128,
                    digest,
                    AES_128_KEY_FLAGS,
                    ctypes.byref(self.key),
                ),
                "The tool failed to derive the firmware encryption key",
            )
        finally:
            if digest.value:
                self.api.CryptDestroyHash(digest)

    @staticmethod
    def _check(api_result, message):
        if not api_result:
            raise ctypes.WinError(ctypes.get_last_error(), message)


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Encrypt or decrypt Fanatec firmware files"
    )
    commands = parser.add_subparsers(dest="operation", required=True)

    encrypt_parser = commands.add_parser("encrypt")
    encrypt_parser.add_argument("input", type=Path)
    encrypt_parser.add_argument("output", type=Path)
    encrypt_parser.add_argument("--header")
    encrypt_parser.add_argument("--legacy-wide-secret", action="store_true")
    encrypt_parser.add_argument("--preserve-newlines", action="store_true")

    decrypt_parser = commands.add_parser("decrypt")
    decrypt_parser.add_argument("input", type=Path)
    decrypt_parser.add_argument("output", type=Path)
    decrypt_parser.add_argument("--legacy-wide-secret", action="store_true")
    decrypt_parser.add_argument("--strip-header", action="store_true")

    return parser.parse_args()


def firmware_secret(use_legacy_wide_secret):
    value = os.environ.get(SECRET_NAME)
    if value is None:
        try:
            value = next(
                line.partition("=")[2]
                for line in ENV_FILE.read_text().splitlines()
                if line.partition("=")[0] == SECRET_NAME
            )
        except (FileNotFoundError, StopIteration) as error:
            raise RuntimeError(f"Set {SECRET_NAME} or add it to {ENV_FILE}") from error

    try:
        secret = bytes.fromhex(value)
    except ValueError as error:
        raise ValueError(f"{SECRET_NAME} must contain hexadecimal bytes") from error
    if len(secret) != AES_BLOCK_SIZE:
        raise ValueError(f"{SECRET_NAME} must contain {AES_BLOCK_SIZE} bytes")
    if use_legacy_wide_secret:
        return secret.decode("ascii").encode("utf-16le")[: len(secret)]
    return secret


def prepare_payload(payload, header, preserve_newlines=False):
    if not payload.startswith(b":"):
        raise ValueError("Input is not a plaintext Intel HEX file")

    line_ending = b"\n"
    if not preserve_newlines:
        payload = b"\n".join(payload.splitlines()) + b"\n"
    elif b"\r\n" in payload:
        line_ending = b"\r\n"
    if header is None:
        return payload

    return header.encode("ascii") + line_ending + payload


def strip_header(payload):
    lines = payload.splitlines(keepends=True)
    first_record = next(
        (index for index, line in enumerate(lines) if line.startswith(b":")), None
    )
    if first_record is None:
        raise ValueError("Decrypted payload does not contain an Intel HEX record")
    return b"".join(lines[first_record:])


def write_atomically(path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", delete=False, dir=path.parent, prefix=f".{path.name}."
        ) as temporary_file:
            temporary_path = Path(temporary_file.name)
            temporary_file.write(payload)
        os.replace(temporary_path, path)
    finally:
        if temporary_path is not None and temporary_path.exists():
            temporary_path.unlink()


def main():
    arguments = parse_arguments()
    secret = firmware_secret(arguments.legacy_wide_secret)

    if arguments.operation == "encrypt":
        payload = arguments.input.read_bytes()
        payload = prepare_payload(
            payload,
            arguments.header,
            arguments.preserve_newlines,
        )
        with FirmwareCipher(secret) as cipher:
            output = cipher.encrypt(payload)
    else:
        with FirmwareCipher(secret) as cipher:
            output = cipher.decrypt(arguments.input.read_bytes())
        if arguments.strip_header:
            output = strip_header(output)

    write_atomically(arguments.output, output)


if __name__ == "__main__":
    main()
