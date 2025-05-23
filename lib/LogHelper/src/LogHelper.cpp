#include <LogHelper.h>
#include <esp_log.h>

void LogHelper::dumpBytes(char const* tag, char const* subtag, uint8_t const* data, size_t len)
{
    if (len == 0) { return; }

    if (esp_log_level_get(tag) < ESP_LOG_VERBOSE) { return; }

    size_t constexpr bytesPerLine = (1<<4); // has to be a power of 2
    char hexdump[bytesPerLine * 3 + 1] = {0};
    char ascii[bytesPerLine + 1] = {0};

    for (size_t i = 0; i <= ((len + bytesPerLine - 1) & ~(bytesPerLine - 1)); ++i) {
        if (i > 0 && i % bytesPerLine == 0) {
            ESP_LOGV(tag, "[%s] %s | %s |", subtag, hexdump, ascii);
            if (i >= len) { return; }
            memset(ascii, 0, sizeof(ascii));
        }

        size_t offset = (i % bytesPerLine)*3;
        char* hexdumpPtr = hexdump + offset;
        size_t remaining = sizeof(hexdump) - offset;
        if (i < len) {
            snprintf(hexdumpPtr, remaining, "%02X ", data[i]);
            if (data[i] >= 32 && data[i] <= 126) {
                ascii[i % 16] = data[i];
            } else {
                ascii[i % 16] = '.';
            }
            continue;
        }

        // add padding for alignment except if we print only a single line
        if (len > bytesPerLine) {
            snprintf(hexdumpPtr, remaining, "   ");
            ascii[i % 16] = ' ';
        }
    }
}
