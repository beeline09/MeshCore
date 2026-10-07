#pragma once

#include <helpers/OptionalFeatureFlags.h>

#ifdef WITH_AEIC_DETECT

#include <stddef.h>
#include <stdint.h>

// Recognises AEIC image chunks sent by MeshCore Open Advanced. The node does
// not reassemble or decode the neural image bitstream; this helper only reads
// the small open chunk header so bubble-ui can show a useful placeholder
// instead of hiding GROUP_DATA 0xAE1C traffic.
namespace aeic {

static const uint16_t kChannelDataType = 0xAE1C;
static const size_t kChunkBlobBytes = 163;
static const size_t kChunkHeaderBytes = 4;
static const size_t kChunkBodyBytes = 158;
static const size_t kParityLengthBytes = 1;
static const size_t kChunkZeroMetadataBytes = 1;
static const uint8_t kMaxDataChunks = 15;

enum class ChunkStatus : uint8_t {
  NotAEIC,
  Malformed,
  Data,
  Parity,
};

struct ChunkInfo {
  ChunkStatus status;
  uint16_t sender_prefix;
  uint8_t image_id;
  uint8_t index;
  uint8_t total;
  uint8_t metadata;
  bool has_metadata;
  uint8_t rate_code;
  uint8_t resolution_code;
  uint8_t aspect_code;

  ChunkInfo();
  bool isData() const { return status == ChunkStatus::Data; }
  bool isParity() const { return status == ChunkStatus::Parity; }
};

bool isChunk(uint16_t data_type, const uint8_t* data, size_t data_length);
ChunkInfo parseChunk(uint16_t data_type, const uint8_t* data, size_t data_length);

const char* rateName(uint8_t rate_code);
const char* resolutionName(uint8_t resolution_code);

}  // namespace aeic

#endif  // WITH_AEIC_DETECT
