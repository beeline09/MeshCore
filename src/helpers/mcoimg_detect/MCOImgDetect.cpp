#include "MCOImgDetect.h"

#ifdef WITH_MCOIMG_DETECT

#include <ctype.h>
#include <string.h>

#include <helpers/Base91.h>

namespace mcoimg_detect {

namespace {

const size_t kMaxTextPayloadBytes = 192;

// A Base91 run on its own is not an image: the alphabet holds every letter and
// digit, so "im:hello", "im:ok" and "im:privet" decode just as happily as an
// image body. What separates a picture from ordinary text is the container's
// subtype in the high nibble of its first byte — the same test the binary
// envelope below has always used. Without it any message that happens to start
// a word with "im:" was rewritten into "<MCOimg image>" for the receiving app.
bool hasBase91Payload(const char* encoded) {
  uint8_t payload[kMaxTextPayloadBytes];
  size_t payload_length = 0;
  return encoded[0] != '\0' &&
         mesh::base91::decode(encoded, strlen(encoded), payload, sizeof(payload), payload_length) &&
         payload_length >= 2 && (payload[0] >> 4) == kImageSubtype;
}

}  // namespace

Meta::Meta() : form(Form::None), version(0), has_explicit_version(false), sender{} {}

static bool readVarUint(const uint8_t*& cursor, const uint8_t* end, size_t& value) {
  value = 0;
  unsigned shift = 0;
  while (cursor < end && shift < sizeof(size_t) * 8) {
    const uint8_t byte = *cursor++;
    if (shift == 28 && sizeof(size_t) == 4 && (byte & 0xF0U) != 0) return false;
    value |= (size_t)(byte & 0x7F) << shift;
    if ((byte & 0x80) == 0) return true;
    shift += 7;
  }
  return false;
}

bool parseText(const char* text, Meta& meta) {
  meta = Meta();
  if (text == nullptr) return false;
  while (*text && isspace((unsigned char)*text)) ++text;
  if (strncmp(text, "im:", 3) == 0 && text[3] != '\0') {
    if (!hasBase91Payload(text + 3)) return false;
    meta.form = Form::LegacyText;
    return true;
  }
  if (text[0] != 'i' || text[1] != 'm') return false;

  const char* cursor = text + 2;
  unsigned version = 0;
  unsigned digits = 0;
  while (*cursor >= '0' && *cursor <= '9') {
    version = version * 10u + (unsigned)(*cursor++ - '0');
    if (++digits > 2 || version > 15u) return false;
  }
  if (digits == 0 || *cursor != ':' || cursor[1] == '\0') return false;
  if (!hasBase91Payload(cursor + 1)) return false;
  meta.form = Form::VersionedText;
  meta.version = (uint8_t)version;
  meta.has_explicit_version = true;
  return true;
}

bool parseBinaryEnvelope(uint16_t data_type, const uint8_t* data,
                         size_t data_length, Meta& meta) {
  meta = Meta();
  if (data_type != kChannelAppDataType || data == nullptr || data_length < 2) return false;

  const uint8_t* cursor = data;
  const uint8_t* end = data + data_length;
  size_t name_length = 0;
  if (!readVarUint(cursor, end, name_length) || name_length >= kMaxNameBytes ||
      (size_t)(end - cursor) < name_length + 2) return false;
  if (name_length > 0) memcpy(meta.sender, cursor, name_length);
  meta.sender[name_length] = '\0';
  cursor += name_length;

  const uint8_t subtype_version = *cursor++;
  if ((subtype_version >> 4) != kImageSubtype || cursor >= end) return false;
  meta.form = Form::Binary;
  meta.version = subtype_version & 0x0F;
  meta.has_explicit_version = true;
  return true;
}

}  // namespace mcoimg_detect

#endif  // WITH_MCOIMG_DETECT
