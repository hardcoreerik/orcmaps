#include "../main/report.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

#define CHECK(condition)      \
  do {                        \
    if (!(condition)) return __LINE__; \
  } while (false)

int main() {
  constexpr char first_path[] = "./orcmaps-report-0001.txt";
  constexpr char second_path[] = "./orcmaps-report-0002.txt";
  std::remove(first_path);
  std::remove(second_path);
  {
    std::ofstream first(first_path);
    first << "original\n";
  }
  const orcmap_demo::Report report{
      320, 170, 4, 3, 1, 1.25, 2.5, 3.75, 4.0, 5.25, 6.5, 7.75,
      30.0, 31.0, 123456, 111, 99, 222, 200, 8388608};

  char saved_path[64];
  CHECK(orcmap_demo::WriteNextReport(".", report, saved_path,
                                     sizeof(saved_path)));
  CHECK(std::string(saved_path) == second_path);

  std::ifstream original(first_path);
  const std::string original_text((std::istreambuf_iterator<char>(original)),
                                  std::istreambuf_iterator<char>());
  CHECK(original_text == "original\n");

  std::ifstream input(second_path);
  const std::string text((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
  CHECK(text.find("display=320x170\n") != std::string::npos);
  CHECK(text.find("visible=4 present=3 missing=1\n") != std::string::npos);
  CHECK(text.find("power_to_map_ms=31.000\n") != std::string::npos);
  CHECK(text.find("bytes_read=123456\n") != std::string::npos);
  CHECK(text.find("RESULT=PASS\n") != std::string::npos);

  char result_line[64];
  orcmap_demo::FormatResultLine(result_line, sizeof(result_line),
                                report.frame_total_ms, true);
  CHECK(std::string(result_line) == "Result: 30 ms  Report saved");

  std::remove(first_path);
  std::remove(second_path);
}
