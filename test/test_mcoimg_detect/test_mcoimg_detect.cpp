#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <helpers/Base91.h>
#include <helpers/mcoimg_detect/MCOImgDetect.h>

namespace {

// An image container as it travels in a text transport: subtype 0x01 in the
// high nibble of the first byte, then a body.
std::string imageToken(const char* prefix, uint8_t subtype_version, size_t body_bytes) {
  std::vector<uint8_t> container(1 + body_bytes, 0xAA);
  container[0] = subtype_version;
  char encoded[256];
  size_t length = 0;
  EXPECT_TRUE(mesh::base91::encode(container.data(), container.size(), encoded,
                                   sizeof(encoded), length));
  return std::string(prefix) + std::string(encoded, length);
}

}  // namespace

TEST(MCOImgDetect, RecognisesTextVersions) {
  mcoimg_detect::Meta meta;
  const std::string versioned = imageToken("im3:", 0x13, 4);
  ASSERT_TRUE(mcoimg_detect::parseText(versioned.c_str(), meta));
  EXPECT_EQ(meta.form, mcoimg_detect::Form::VersionedText);
  EXPECT_EQ(meta.version, 3);
  EXPECT_TRUE(meta.has_explicit_version);

  const std::string legacy = " " + imageToken("im:", 0x13, 4);
  ASSERT_TRUE(mcoimg_detect::parseText(legacy.c_str(), meta));
  EXPECT_EQ(meta.form, mcoimg_detect::Form::LegacyText);
  EXPECT_FALSE(meta.has_explicit_version);
  EXPECT_FALSE(mcoimg_detect::parseText("im3:", meta));
  EXPECT_FALSE(mcoimg_detect::parseText("im: plain text", meta));
  EXPECT_FALSE(mcoimg_detect::parseText("im3:abc def", meta));
  EXPECT_FALSE(mcoimg_detect::parseText("plain", meta));
}

// The "im:" prefix is not proof of an image — every letter and digit is in the
// Base91 alphabet, so ordinary words run through the decoder. These used to be
// reported as images (and rewritten to "<MCOimg image>" for the app).
TEST(MCOImgDetect, PlainTextIsNotAnImage) {
  const char* plain[] = {
      "im:hello",
      "im:ok",
      "im:privet",
      "im:dad",
      "im3:abc",
      "im4:QrSt",
      "im: \xd0\xbf\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82",
      "im:hello world",
  };
  mcoimg_detect::Meta meta;
  for (const char* text : plain) EXPECT_FALSE(mcoimg_detect::parseText(text, meta)) << text;

  // A Base91 run that is not container-shaped stays text ...
  EXPECT_FALSE(mcoimg_detect::parseText(imageToken("im:", 0x5A, 4).c_str(), meta));
  // ... while a container with a version this firmware does not know is still
  // an image, just an unreadable one.
  EXPECT_TRUE(mcoimg_detect::parseText(imageToken("im4:", 0x14, 4).c_str(), meta));
}

TEST(MCOImgDetect, RecognisesBinaryEnvelope) {
  const uint8_t data[] = {3, 'B', 'o', 'b', 0x14, 0xAA};
  mcoimg_detect::Meta meta;
  ASSERT_TRUE(mcoimg_detect::parseBinaryEnvelope(0x0120, data, sizeof(data), meta));
  EXPECT_EQ(meta.form, mcoimg_detect::Form::Binary);
  EXPECT_EQ(meta.version, 4);
  EXPECT_STREQ(meta.sender, "Bob");
  EXPECT_FALSE(mcoimg_detect::parseBinaryEnvelope(0x0121, data, sizeof(data), meta));
}

TEST(MCOImgDetect, RejectsMalformedBinaryEnvelope) {
  const uint8_t wrong_subtype[] = {0, 0x24, 0xAA};
  const uint8_t missing_body[] = {0, 0x14};
  const uint8_t bad_name[] = {4, 'B', 'o', 'b'};
  const uint8_t huge_name[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x14, 0xAA};
  mcoimg_detect::Meta meta;
  EXPECT_FALSE(mcoimg_detect::parseBinaryEnvelope(0x0120, wrong_subtype,
                                                   sizeof(wrong_subtype), meta));
  EXPECT_FALSE(mcoimg_detect::parseBinaryEnvelope(0x0120, missing_body,
                                                   sizeof(missing_body), meta));
  EXPECT_FALSE(mcoimg_detect::parseBinaryEnvelope(0x0120, bad_name,
                                                   sizeof(bad_name), meta));
  EXPECT_FALSE(mcoimg_detect::parseBinaryEnvelope(0x0120, huge_name,
                                                   sizeof(huge_name), meta));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
