#include "compiler/arena.hpp"
#include "compiler/lexer.hpp"
#include "compiler/token.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cout << "usage: " << argv[0] << "\n";
    return 1;
  }

  std::ifstream file(argv[1]);
  if (!file.is_open()) {
    std::cerr << "file open error: " << argv[1] << "\n";
    return 1;
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string source = buffer.str();
  // std::cout << source;

  vd::Lexer lexer(source);
  while (true) {
    auto token = lexer.next_token();
    std::cout << token << "\n";
    if (token.kind == vd::TokenKind::_EOF)
      break;
  }
  return 0;
}
