#pragma once

#include "token.hpp"
#include "tree_builder.hpp"
#include <cstddef>
#include <iostream>
#include <memory_resource>
#include <ostream>
#include <string>
#include <vector>

namespace vd {

struct ParserError {
  std::string message;
  SourceLocation loc;
};

inline std::ostream &operator<<(std::ostream &os, const ParserError &err) {
  return os << "error: " << err.message << " at " << err.loc.line << ":"
            << err.loc.col;
}

class Parser {

private:
  std::span<const Token> tokens_;
  std::size_t cursor_ = 0;

  std::pmr::vector<Event> events_;

  std::size_t token_budget_;
  static constexpr size_t TOKEN_BUDGET = 10'000'000;

public:
  // public for error printing
  std::pmr::vector<ParserError> errors_;

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

  bool expect(TokenKind kind) {
    if (eat(kind))
      return true;
    SourceLocation loc =
        cursor_ < tokens_.size() ? tokens_[cursor_].loc : SourceLocation{};
    errors_.push_back({token_name(kind), loc});
    return false;
  }

  void bump() {
    if (peek() == TokenKind::_EOF)
      return;
    events_.push_back(Event{EventKind::TOKEN,
                            static_cast<SyntaxKind>(tokens_[cursor_].kind)});
    cursor_++;
  }

  // --- TREE CONSTRUCTION ---
  std::size_t open_marker() {
    std::size_t mark = events_.size();
    events_.push_back(Event{EventKind::OPEN, SyntaxKind::ERROR});
    return mark;
  }

  void close(std::size_t mark, SyntaxKind kind) {
    events_[mark].syntax_kind = kind;
    events_.push_back(Event{EventKind::CLOSE, kind});
  }

  std::size_t checkpoint() const { return events_.size(); }

  // --- RAII Node ---
  struct OpenNode {
    Parser &parser;
    std::size_t marker;
    SyntaxKind kind;

    OpenNode(Parser &p, std::size_t m)
        : parser(p), marker(m), kind(SyntaxKind::ERROR) {}
    ~OpenNode() { parser.close(marker, kind); }
    void set_kind(SyntaxKind k) { kind = k; }

    OpenNode(const OpenNode &) = delete;
    OpenNode &operator=(const OpenNode &) = delete;
  };

  OpenNode open() { return OpenNode(*this, open_marker()); }

public:
  void parse_expr(int min_bp = 0, bool allow_struct_lit = true);
  void parse_primary(bool allow_struct_lit = true);

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

  void amend_last_error(const char *msg);
  void error_recover(const char *msg);

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

  // was too eager with previous def
  bool is_fn_def() const {
    size_t i = 0;
    if (peek(i) == TokenKind::EXPORT)
      i++;

    if (peek(i) != TokenKind::IDENT)
      return false;
    i++;

    // optional generics
    if (peek(i) == TokenKind::OBRACK) {
      int depth = 1;
      i++;
      while (depth > 0 && peek(i) != TokenKind::_EOF) {
        if (peek(i) == TokenKind::OBRACK)
          depth++;
        else if (peek(i) == TokenKind::CBRACK)
          depth--;
        i++;
      }
    }

    if (peek(i) != TokenKind::OPAREN)
      return false;
    i++;

    int depth = 1;
    while (depth > 0 && peek(i) != TokenKind::_EOF) {
      if (peek(i) == TokenKind::OPAREN)
        depth++;
      else if (peek(i) == TokenKind::CPAREN)
        depth--;
      i++;
    }

    return peek(i) == TokenKind::ARROW;
  }

  // let x = ..., mut x = ...
  bool is_inferred_binding() const {
    size_t i = 0;
    if (peek(i) == TokenKind::LET)
      return true;
    if (peek(i) == TokenKind::MUT) {
      i++;
      // After 'mut', must have an identifier or '('
      if (peek(i) == TokenKind::IDENT || peek(i) == TokenKind::OPAREN)
        return true;
    }
    return false;
  }

  /// <type_name> x = ..., mut <type_name> x =
  bool is_typed_binding() const {
    size_t i = 0;
    if (peek(i) == TokenKind::MUT)
      i++;

    // tuple type destructuring: (int x, int y) = ...
    if (peek(i) == TokenKind::OPAREN) {
      int depth = 1;
      i++;
      while (depth > 0 && peek(i) != TokenKind::_EOF) {
        if (peek(i) == TokenKind::OPAREN)
          depth++;
        else if (peek(i) == TokenKind::CPAREN)
          depth--;
        i++;
      }
      // i is now past the closing )
      return peek(i) == TokenKind::ASSIGN || peek(i) == TokenKind::OBRACE;
    }

    // array/slice type: [int; 4] or &[int]
    if (peek(i) == TokenKind::OBRACK || peek(i) == TokenKind::AMP) {
      int depth = 0;
      while (peek(i) != TokenKind::_EOF) {
        if (peek(i) == TokenKind::OBRACK)
          depth++;
        else if (peek(i) == TokenKind::CBRACK) {
          depth--;
          if (depth == 0) {
            i++;
            break;
          }
        }
        i++;
      }
      if (peek(i) != TokenKind::IDENT)
        return false;
      i++;
      return peek(i) == TokenKind::ASSIGN || peek(i) == TokenKind::OBRACE;
    }

    // Named type: int, Vec[int], etc.
    if (peek(i) == TokenKind::IDENT) {
      i++;
      if (peek(i) == TokenKind::OBRACK) {
        int depth = 1;
        i++;
        while (depth > 0 && peek(i) != TokenKind::_EOF) {
          if (peek(i) == TokenKind::OBRACK)
            depth++;
          else if (peek(i) == TokenKind::CBRACK)
            depth--;
          i++;
        }
      }
      if (peek(i) == TokenKind::QUESTION)
        i++;
      if (peek(i) != TokenKind::IDENT)
        return false;
      i++;
      return peek(i) == TokenKind::ASSIGN || peek(i) == TokenKind::OBRACE;
    }
    return false;
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

  bool check_budget() {
    if (--token_budget_ == 0) {
      errors_.push_back({"parser exceeded token budget", current().loc});
      return false;
    }
    return true;
  }
};

} // namespace vd
