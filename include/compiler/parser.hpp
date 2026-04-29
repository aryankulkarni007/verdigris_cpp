#pragma once

#include "compiler/token.hpp"
#include "compiler/tree_builder.hpp"
#include <memory_resource>
#include <span>
#include <vector>

namespace vd {

struct ParserError {
  std::string_view message;
  SourceLocation loc;
};

class Parser {
public:
  // NOTE: make a separate parser arena or take main arena
  Parser(std::span<const Token> tokens, std::pmr::memory_resource *resource)
      : tokens_(tokens), events_(resource), errors_(resource) {}

  // --- CORE API ---
  TokenKind peek(std::size_t offset = 0) const {
    if (cursor_ + offset >= tokens_.size())
      return TokenKind::_EOF;
    return tokens_[cursor_ + offset].kind;
  }

  const Token &current() const {
    if (cursor_ >= tokens_.size())
      return tokens_.back(); // EOF token
    return tokens_[cursor_];
  }

  bool eat(TokenKind kind) {
    if (peek() == kind) {
      bump();
      return true;
    }
    return false;
  }

  void expect(TokenKind kind) {
    if (!eat(kind)) {
      SourceLocation loc =
          cursor_ < tokens_.size() ? tokens_[cursor_].loc : SourceLocation{};
      errors_.push_back({token_name(kind), loc}); // "expected X"
    }
  }

  void bump() {
    if (peek() == TokenKind::_EOF)
      return;
    events_.push_back(Event{EventKind::TOKEN,
                            static_cast<SyntaxKind>(tokens_[cursor_].kind)});
    cursor_++;
  }

  // --- TREE CONSTRUCTION ---
  std::size_t open() {
    std::size_t mark = events_.size();
    events_.push_back(Event{EventKind::OPEN, SyntaxKind::ERROR});
    return mark;
  }

  void close(std::size_t mark, SyntaxKind kind) {
    events_[mark].syntax_kind = kind;
    events_.push_back(Event{EventKind::CLOSE, kind});
  }

  std::size_t checkpoint() const { return events_.size(); }

private:
  std::span<const Token> tokens_;
  std::size_t cursor_ = 0;

  std::pmr::vector<Event> events_;
  std::pmr::vector<ParserError> errors_;
};

} // namespace vd
