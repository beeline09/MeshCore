#include <gtest/gtest.h>

#include <helpers/aeic/AEICDetect.h>

TEST(AEICDetect, RecognisesChunkZero) {
  const uint8_t chunk[] = {
    0x12, 0x34, 0x56, 0x02,  // sender, image id, idx 0 / total 2
    0x00,                    // aspect 0, resolution 0 = 512, rate 0 = ft32
    0xAA,
  };

  EXPECT_TRUE(aeic::isChunk(aeic::kChannelDataType, chunk, sizeof(chunk)));
  const aeic::ChunkInfo info =
      aeic::parseChunk(aeic::kChannelDataType, chunk, sizeof(chunk));
  EXPECT_TRUE(info.isData());
  EXPECT_FALSE(info.isParity());
  EXPECT_TRUE(info.has_metadata);
  EXPECT_EQ(info.sender_prefix, 0x1234u);
  EXPECT_EQ(info.image_id, 0x56u);
  EXPECT_EQ(info.index, 0u);
  EXPECT_EQ(info.total, 2u);
  EXPECT_EQ(info.rate_code, 0u);
  EXPECT_EQ(info.resolution_code, 0u);
  EXPECT_STREQ(aeic::rateName(info.rate_code), "ft32");
  EXPECT_STREQ(aeic::resolutionName(info.resolution_code), "512");
}

TEST(AEICDetect, RecognisesDataAndParityChunks) {
  const uint8_t data_chunk[] = {
    0x12, 0x34, 0x56, 0x12,  // idx 1 / total 2
    0xAA,
  };
  const uint8_t parity_chunk[] = {
    0x12, 0x34, 0x56, 0x22,  // idx 2 / total 2
    0x9D,
  };

  aeic::ChunkInfo info =
      aeic::parseChunk(aeic::kChannelDataType, data_chunk, sizeof(data_chunk));
  EXPECT_TRUE(info.isData());
  EXPECT_FALSE(info.has_metadata);
  EXPECT_EQ(info.index, 1u);
  EXPECT_EQ(info.total, 2u);

  info = aeic::parseChunk(aeic::kChannelDataType, parity_chunk, sizeof(parity_chunk));
  EXPECT_TRUE(info.isParity());
  EXPECT_EQ(info.index, 2u);
  EXPECT_EQ(info.total, 2u);
}

TEST(AEICDetect, RejectsMalformedAndForeignPayloads) {
  const uint8_t valid[] = {0x12, 0x34, 0x56, 0x01, 0x00};
  EXPECT_FALSE(aeic::isChunk(0x0120, valid, sizeof(valid)));
  EXPECT_EQ((int)aeic::parseChunk(0x0120, valid, sizeof(valid)).status,
            (int)aeic::ChunkStatus::NotAEIC);

  const uint8_t too_short[] = {0x12, 0x34, 0x56};
  EXPECT_FALSE(aeic::isChunk(aeic::kChannelDataType, too_short, sizeof(too_short)));

  const uint8_t zero_total[] = {0x12, 0x34, 0x56, 0x00};
  EXPECT_FALSE(aeic::isChunk(aeic::kChannelDataType, zero_total, sizeof(zero_total)));

  const uint8_t index_past_total[] = {0x12, 0x34, 0x56, 0x31};
  EXPECT_FALSE(aeic::isChunk(aeic::kChannelDataType, index_past_total,
                             sizeof(index_past_total)));

  const uint8_t empty_chunk_zero[] = {0x12, 0x34, 0x56, 0x01};
  EXPECT_FALSE(aeic::isChunk(aeic::kChannelDataType, empty_chunk_zero,
                             sizeof(empty_chunk_zero)));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
