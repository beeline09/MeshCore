#include <helpers/aeic/AEICDetect.h>

#ifdef WITH_AEIC_DETECT

namespace aeic {

ChunkInfo::ChunkInfo()
    : status(ChunkStatus::NotAEIC), sender_prefix(0), image_id(0), index(0),
      total(0), metadata(0), has_metadata(false), rate_code(0),
      resolution_code(0), aspect_code(0) {}

bool isChunk(uint16_t data_type, const uint8_t* data, size_t data_length) {
  const ChunkInfo info = parseChunk(data_type, data, data_length);
  return info.status == ChunkStatus::Data || info.status == ChunkStatus::Parity;
}

ChunkInfo parseChunk(uint16_t data_type, const uint8_t* data, size_t data_length) {
  ChunkInfo info;
  if (data_type != kChannelDataType) return info;

  info.status = ChunkStatus::Malformed;
  if (data == nullptr) return info;
  if (data_length < kChunkHeaderBytes || data_length > kChunkBlobBytes) return info;

  info.sender_prefix = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
  info.image_id = data[2];
  info.index = (uint8_t)((data[3] >> 4) & 0x0F);
  info.total = (uint8_t)(data[3] & 0x0F);
  if (info.total == 0 || info.total > kMaxDataChunks) return info;
  if (info.index > info.total) return info;

  const size_t body_length = data_length - kChunkHeaderBytes;
  if (info.index == info.total) {
    if (body_length < kParityLengthBytes) return info;
    if (body_length > kChunkBodyBytes + kParityLengthBytes) return info;
    info.status = ChunkStatus::Parity;
    return info;
  }

  if (body_length > kChunkBodyBytes) return info;
  if (info.index == 0) {
    if (body_length < kChunkZeroMetadataBytes) return info;
    info.metadata = data[kChunkHeaderBytes];
    info.rate_code = (uint8_t)(info.metadata & 0x03);
    info.resolution_code = (uint8_t)((info.metadata >> 2) & 0x03);
    info.aspect_code = (uint8_t)((info.metadata >> 4) & 0x0F);
    // Unknown rate/resolution values mean a future AEIC variant. This is
    // still an image chunk, just not one this firmware can fully name.
    info.has_metadata = true;
  }
  info.status = ChunkStatus::Data;
  return info;
}

const char* rateName(uint8_t rate_code) {
  switch (rate_code) {
    case 0: return "ft32";
    case 1: return "ft16";
    default: return "?";
  }
}

const char* resolutionName(uint8_t resolution_code) {
  switch (resolution_code) {
    case 0: return "512";
    case 1: return "256";
    case 2: return "768";
    case 3: return "1024";
    default: return "?";
  }
}

}  // namespace aeic

#endif  // WITH_AEIC_DETECT
