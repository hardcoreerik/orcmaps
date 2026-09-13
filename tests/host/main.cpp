#include <cstdio>
#include <string>

#include "test_util.hpp"

void RunGeoTests();
void RunPmTilesTests(const std::string& fixture_path);
void RunStyleTests();
void RunAttributionTests();

int main(int argc, char** argv) {
  const std::string fixture_path =
      argc > 1 ? argv[1] : "tests/fixtures/tiny.pmtiles";

  RunGeoTests();
  RunPmTilesTests(fixture_path);
  RunStyleTests();
  RunAttributionTests();

  if (orcmap::test::g_failures == 0) {
    std::printf("PASS: all OrcMaps host tests passed\n");
    return 0;
  }
  std::printf("FAIL: %d OrcMaps host test assertion(s) failed\n",
              orcmap::test::g_failures);
  return 1;
}
