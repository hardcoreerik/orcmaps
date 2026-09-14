#include <cstdio>
#include <string>

#include "test_util.hpp"

void RunGeoTests();
void RunPmTilesTests(const std::string& fixture_path);
void RunStyleTests();
void RunAttributionTests();
void RunMvtTests(const std::string& fixture_path);
void RunFeatureTests(const std::string& mvt_fixture_path);
void RunViewportTests();
void RunRenderTests(const std::string& mvt_fixture_path);

int main(int argc, char** argv) {
  const std::string fixture_path =
      argc > 1 ? argv[1] : "tests/fixtures/tiny.pmtiles";
  const std::string mvt_fixture_path =
      argc > 2 ? argv[2] : "tests/fixtures/tiny.mvt";

  RunGeoTests();
  RunPmTilesTests(fixture_path);
  RunStyleTests();
  RunAttributionTests();
  RunMvtTests(mvt_fixture_path);
  RunFeatureTests(mvt_fixture_path);
  RunViewportTests();
  RunRenderTests(mvt_fixture_path);

  if (orcmap::test::g_failures == 0) {
    std::printf("PASS: all OrcMaps host tests passed\n");
    return 0;
  }
  std::printf("FAIL: %d OrcMaps host test assertion(s) failed\n",
              orcmap::test::g_failures);
  return 1;
}
