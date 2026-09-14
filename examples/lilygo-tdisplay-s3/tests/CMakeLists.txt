cmake_minimum_required(VERSION 3.21)
project(orcmaps_lilygo_report_test LANGUAGES CXX)

enable_testing()
add_executable(report_test report_test.cpp)
target_compile_features(report_test PRIVATE cxx_std_17)
add_test(NAME report_test COMMAND report_test)
