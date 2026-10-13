#include "Path.h"
#include <string.h>
#include <strings.h>
#include <stdint.h>
namespace TransferPath {
bool temporary(const char* name) { return strncasecmp(name, ".transfer-", 10) == 0; }
namespace {
int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
}
bool decode(const char* encoded, char* output, size_t capacity) {
    size_t length = 0;
    for (size_t i = 0; encoded[i]; ++i) {
        unsigned char c = encoded[i];
        if (c == '%') {
            if (!encoded[i+1] || !encoded[i+2]) return false;
            const int a = hex(encoded[i+1]), b = hex(encoded[i+2]);
            if (a < 0 || b < 0) return false;
            c = (a << 4) | b; i += 2;
        } else if (c == '+') c = ' ';
        if (length + 1 >= capacity || c < 32 || c == 127 || c == '\\' ||
            strchr(":*?\"<>|", c)) return false;
        output[length++] = c;
    }
    output[length] = 0;
    if (!length || output[0] != '/') return false;
    // UTF-8 strikt pruefen (keine ueberlangen Sequenzen/Surrogates).
    for (size_t i = 0; i < length; ) {
        const unsigned char lead = output[i++];
        if (lead < 128) continue;
        unsigned count; uint32_t code, minimum;
        if (lead >= 0xc2 && lead <= 0xdf) { count = 1; code = lead & 31; minimum = 0x80; }
        else if (lead >= 0xe0 && lead <= 0xef) { count = 2; code = lead & 15; minimum = 0x800; }
        else if (lead >= 0xf0 && lead <= 0xf4) { count = 3; code = lead & 7; minimum = 0x10000; }
        else return false;
        if (i + count > length) return false;
        while (count--) {
            const unsigned char next = output[i++];
            if ((next & 0xc0) != 0x80) return false;
            code = (code << 6) | (next & 63);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
    }
    if (length == 1) return true;
    const char* segment = output + 1;
    while (*segment) {
        const char* end = strchr(segment, '/');
        const size_t count = end ? size_t(end - segment) : strlen(segment);
        if (!count || count >= 128 || segment[count-1] == '.' || segment[count-1] == ' ' ||
            segment[0] == ' ' || temporary(segment) ||
            (count == 1 && segment[0] == '.') ||
            (count == 2 && segment[0] == '.' && segment[1] == '.')) return false;
        if (!end) break;
        segment = end + 1;
        if (!*segment) return false;
    }
    return true;
}
}
