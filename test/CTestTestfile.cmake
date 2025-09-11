# CMake generated Testfile for 
# Source directory: /Users/vobbilis/go/src/github.com/vobbilis/codegen/mytsdb/test
# Build directory: /Users/vobbilis/go/src/github.com/vobbilis/codegen/mytsdb/test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(PromQLParserTests "/Users/vobbilis/go/src/github.com/vobbilis/codegen/mytsdb/test/promql_parser_tests")
set_tests_properties(PromQLParserTests PROPERTIES  _BACKTRACE_TRIPLES "/Users/vobbilis/go/src/github.com/vobbilis/codegen/mytsdb/test/CMakeLists.txt;64;add_test;/Users/vobbilis/go/src/github.com/vobbilis/codegen/mytsdb/test/CMakeLists.txt;0;")
subdirs("unit")
subdirs("integration")
subdirs("benchmark")
subdirs("load")
subdirs("stress")
subdirs("scalability")
