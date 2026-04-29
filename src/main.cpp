// #include "compiler/arena.hpp"
#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/token.hpp"
#include <cstddef>
#include <fcntl.h>
#include <iostream>
#include <memory_resource>
#include <sys/mman.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cout << "usage: " << argv[0] << " <path_to_file> " << "\n";
    return 1;
  }

  // no allocation file read
  int fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    std::cerr << "Error opening " << argv[1] << ": " << std::strerror(errno)
              << "\n";
    return 1;
  }
  struct stat st;
  fstat(fd, &st);
  // map as a read-only buffer
  const char *mapped_data =
      (const char *)mmap(nullptr, static_cast<std::size_t>(st.st_size),
                         PROT_READ, MAP_PRIVATE, fd, 0);

  std::string_view source(mapped_data, static_cast<std::size_t>(st.st_size));
  // std::cout << source;

  // 32kb init size
  char buffer_space[32768];
  std::pmr::monotonic_buffer_resource arena(buffer_space, sizeof(buffer_space));

  vd::Lexer lexer(source);

  std::pmr::vector<vd::Token> ts(&arena);
  ts.reserve(source.size() / 5);

  while (true) {
    auto token = lexer.next_token();
    // std::cout << token << "\n";
    ts.push_back(token);
    if (token.kind == vd::TokenKind::_EOF)
      break;
  }

  vd::Parser parser(ts, &arena);
  parser.parse_expr();
  parser.raw_dump();
  return 0;
}
