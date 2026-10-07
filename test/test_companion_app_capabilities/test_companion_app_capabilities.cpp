#include <gtest/gtest.h>

#include <helpers/CompanionAppCapabilities.h>

TEST(CompanionAppCapabilities, ParsesCommaSeparatedList) {
  const char app[] = "MeshCoreOpen;cap=frmfrg1,mctxt,mcmp,mcimg,aeic";
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(app);
  const size_t length = sizeof(app) - 1;
  EXPECT_TRUE(companion_app::hasCapability(bytes, length, "mctxt"));
  EXPECT_TRUE(companion_app::hasCapability(bytes, length, "mcmp"));
  EXPECT_TRUE(companion_app::hasCapability(bytes, length, "mcimg"));
  EXPECT_TRUE(companion_app::hasCapability(bytes, length, "aeic"));
  EXPECT_FALSE(companion_app::hasCapability(bytes, length, "mct"));
  EXPECT_FALSE(companion_app::hasCapability(bytes, length, "unknown"));
}

TEST(CompanionAppCapabilities, StopsAtNextSemicolonField) {
  const char app[] = "Client;cap=mctxt,mcmp;note=aeic";
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(app);
  EXPECT_TRUE(companion_app::hasCapability(bytes, sizeof(app) - 1, "mctxt"));
  EXPECT_FALSE(companion_app::hasCapability(bytes, sizeof(app) - 1, "aeic"));
}

TEST(CompanionAppCapabilities, RecognizesMeshCoreOpenNameForAEICFallback) {
  const char compact[] = "MeshCoreOpen";
  const char spaced[] = "meshcore open;cap=mctxt";
  const char other[] = "OtherClient;cap=mctxt";
  EXPECT_TRUE(companion_app::appNameIsMeshCoreOpen(
      reinterpret_cast<const uint8_t*>(compact), sizeof(compact) - 1));
  EXPECT_TRUE(companion_app::appNameIsMeshCoreOpen(
      reinterpret_cast<const uint8_t*>(spaced), sizeof(spaced) - 1));
  EXPECT_FALSE(companion_app::appNameIsMeshCoreOpen(
      reinterpret_cast<const uint8_t*>(other), sizeof(other) - 1));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
