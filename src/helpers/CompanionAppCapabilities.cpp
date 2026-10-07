#include "CompanionAppCapabilities.h"

#include <string.h>

namespace companion_app {
namespace {

char lowercaseAscii(char c) {
  return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

bool compactTokenEquals(const uint8_t* token, size_t token_len,
                        const char* compact_lowercase) {
  size_t want = 0;
  for (size_t i = 0; i < token_len; i++) {
    const char c = (char)token[i];
    if (c == 0 || c == ';') break;
    if (c == '_' || c == '-' || c == ' ') continue;
    if (compact_lowercase[want] == 0 ||
        lowercaseAscii(c) != compact_lowercase[want]) return false;
    want++;
  }
  return compact_lowercase[want] == 0;
}

}  // namespace

bool hasCapability(const uint8_t* app_name, size_t app_name_len,
                   const char* capability) {
  if (!app_name || !capability || capability[0] == 0) return false;
  const size_t wanted_len = strlen(capability);
  size_t pos = 0;
  while (pos < app_name_len && app_name[pos] != 0) {
    const size_t start = pos;
    while (pos < app_name_len && app_name[pos] != 0 && app_name[pos] != ';') pos++;
    static const char CAP_PREFIX[] = "cap=";
    const size_t token_len = pos - start;
    if (token_len >= sizeof(CAP_PREFIX) - 1 &&
        memcmp(&app_name[start], CAP_PREFIX, sizeof(CAP_PREFIX) - 1) == 0) {
      size_t cap_pos = start + sizeof(CAP_PREFIX) - 1;
      while (cap_pos < pos) {
        const size_t cap_start = cap_pos;
        while (cap_pos < pos && app_name[cap_pos] != ',') cap_pos++;
        if (cap_pos - cap_start == wanted_len &&
            memcmp(&app_name[cap_start], capability, wanted_len) == 0) return true;
        if (cap_pos < pos && app_name[cap_pos] == ',') cap_pos++;
      }
    }
    if (pos < app_name_len && app_name[pos] == ';') pos++;
  }
  return false;
}

bool appNameIsMeshCoreOpen(const uint8_t* app_name, size_t app_name_len) {
  if (!app_name) return false;
  size_t first_token_len = 0;
  while (first_token_len < app_name_len &&
         app_name[first_token_len] != 0 &&
         app_name[first_token_len] != ';') {
    first_token_len++;
  }
  return compactTokenEquals(app_name, first_token_len, "meshcoreopen");
}

}  // namespace companion_app
