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

private:
  std::span<const Token> tokens_;
  std::size_t cursor_ = 0;

  std::pmr::vector<Event> events_;
  std::pmr::vector<ParserError> errors_;

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

public:
  void parse_primary();
  void parse_expr(int min_bp = 0);

  void parse_type_ref();
  void parse_typed_binding();
  void parse_inferred_binding();

  void parse_return_stmt();
  void parse_block();
  bool parse_stmt();

  void parse_args();
  void parse_gen_args();
  void parse_gen_params();
  void parse_fn();
  void parse_field();
  void parse_implement();

  void parse_structure();
  void parse_params();

  void parse_variant();
  void parse_type_def();

  void parse_interface();

  void parse_if_expr();

  void parse_pattern();
  void parse_match_arm();
  void parse_match_expr();

  void parse_for_stmt();
  void parse_while_stmt();
  void parse_loop_stmt();
  void parse_break_stmt();
  void parse_continue_stmt();
  void parse_struct_lit();
  void parse_file();

  void raw_dump() const;

  // parser equivalent to consume while
  template <typename F> void advance_while(F &&predicate) {
    while (peek() != TokenKind::_EOF && predicate(peek())) {
      bump();
    }
  }

  void error_recover();

private:
  int left_bp(TokenKind k);

  bool is_struct_lit() const {
    if (peek(0) != TokenKind::IDENT)
      return false;
    // IDENT {
    if (peek(1) == TokenKind::OBRACE)
      return true;
    // IDENT [ ... ] {  — generic struct lit
    if (peek(1) == TokenKind::OBRACK) {
      // scan past brackets
      std::size_t i = 2;
      int depth = 1;
      while (cursor_ + i < tokens_.size() && depth > 0) {
        if (peek(i) == TokenKind::OBRACK)
          depth++;
        else if (peek(i) == TokenKind::CBRACK)
          depth--;
        i++;
      }
      return peek(i) == TokenKind::OBRACE;
    }
    return false;
  }

  inline bool is_fn_def() const {
    if (peek() == TokenKind::IDENT && peek(1) == TokenKind::OPAREN)
      return true;
    return false;
  }

  // let x = ..., mut x = ...
  bool is_inferred_binding() const {
    if (peek() == TokenKind::LET)
      return true;
    if (peek() == TokenKind::MUT && peek(1) != TokenKind::IDENT)
      return false;
    if (peek() == TokenKind::MUT && peek(1) == TokenKind::IDENT &&
        peek(2) == TokenKind::ASSIGN)
      return true;
    return false;
  }

  // <type_name> x = ..., mut <type_name> x =
  bool is_typed_binding() const {
    size_t i = 0;
    if (peek(i) == TokenKind::MUT)
      ++i;
    return peek(i) == TokenKind::IDENT && peek(i + 1) == TokenKind::IDENT;
  }

  bool is_right_assoc(TokenKind k) const {
    switch (k) {
    case TokenKind::ASSIGN:
    case TokenKind::PLUSEQ:
    case TokenKind::MINUSEQ:
    case TokenKind::STAREQ:
    case TokenKind::SLASHEQ:
    case TokenKind::MODULOEQ:
      return true;
    default:
      return false;
    }
  }
};

} // namespace vd
