#pragma once

#include <helpers/OptionalFeatureFlags.h>

#ifdef WITH_MCOIMG_DETECT

#include <stddef.h>
#include <stdint.h>

namespace mcoimg_detect {

static const uint16_t kChannelAppDataType = 0x0120;
static const uint8_t kImageSubtype = 0x01;
static const size_t kMaxNameBytes = 32;

enum class Form : uint8_t { None, LegacyText, VersionedText, Binary };

struct Meta {
  Form form;
  uint8_t version;
  bool has_explicit_version;
  char sender[kMaxNameBytes];

  Meta();
};

bool parseText(const char* text, Meta& meta);
bool parseBinaryEnvelope(uint16_t data_type, const uint8_t* data,
                         size_t data_length, Meta& meta);

}  // namespace mcoimg_detect

#endif  // WITH_MCOIMG_DETECT
