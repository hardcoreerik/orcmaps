#include "../main/report.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

int main() {
  constexpr char path[] = "orcmaps-report-test.txt";
  const orcmap_demo::Report report{
      320, 170, 4, 3, 1, 1.25, 2.5, 3.75, 4.0, 5.25, 6.5, 7.75,
      30.0, 31.0, 123456, 111, 99, 222, 200, 8388608};

  assert(orcmap_demo::WriteReport(path, report));
  std::ifstream input(path);
  const std::string text((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
  assert(text.find("display=320x170\n") != std::string::npos);
  assert(text.find("visible=4 present=3 missing=1\n") != std::string::npos);
  assert(text.find("power_to_map_ms=31.000\n") != std::string::npos);
  assert(text.find("bytes_read=123456\n") != std::string::npos);
  assert(text.find("RESULT=PASS\n") != std::string::npos);
  std::remove(path);
}
