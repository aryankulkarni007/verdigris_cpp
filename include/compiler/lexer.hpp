#pragma once

#include "compiler/token.hpp"
#include <array>
#include <cstddef>
#include <string_view>
#include <unordered_map>

static vd::TokenKind keyword_kind(std::string_view s) {
  if (s == "as")
    return vd::TokenKind::AS;
  if (s == "let")
    return vd::TokenKind::LET;
  if (s == "mut")
    return vd::TokenKind::MUT;
  if (s == "struct")
    return vd::TokenKind::STRUCT;
  if (s == "enum")
    return vd::TokenKind::ENUM;
  if (s == "for")
    return vd::TokenKind::FOR;
  if (s == "in")
    return vd::TokenKind::IN;
  if (s == "match")
    return vd::TokenKind::MATCH;
  if (s == "loop")
    return vd::TokenKind::LOOP;
  if (s == "while")
    return vd::TokenKind::WHILE;
  if (s == "break")
    return vd::TokenKind::BREAK;
  if (s == "continue")
    return vd::TokenKind::CONTINUE;
  if (s == "if")
    return vd::TokenKind::IF;
  if (s == "else")
    return vd::TokenKind::ELSE;
  if (s == "return")
    return vd::TokenKind::RETURN;
  if (s == "null")
    return vd::TokenKind::_NULL;
  if (s == "self")
    return vd::TokenKind::SELF;
  if (s == "pub")
    return vd::TokenKind::PUB;
  if (s == "import")
    return vd::TokenKind::IMPORT;
  if (s == "interface")
    return vd::TokenKind::INTERFACE;
  if (s == "defer")
    return vd::TokenKind::DEFER;
  if (s == "catch")
    return vd::TokenKind::CATCH;
  return vd::TokenKind::IDENT;
}

namespace vd {
class Lexer {
public:
  explicit Lexer(std::string_view source) : source_(source) {
    initialise_dispatch_table();
  }

  Token next_token() {
    std::string_view leading_trivia = lex_trivia();
    if (is_at_end())
      return anchor(leading_trivia).complete(TokenKind::_EOF);

    auto handler = dispatch_[static_cast<unsigned char>(peek())];
    return (this->*handler)(leading_trivia);
  }

private:
  // --- DATA ---
  struct LexError {
    enum class Kind {
      UnterminatedBlockComment,
      UnterminatedString,
      InvalidCharacter
    };
    Kind kind;
    SourceLocation loc;
  };

  std::string_view source_;
  std::size_t cursor_ = 0;

  uint32_t line_ = 1;
  uint32_t col_ = 1;

  // lexer errors
  std::vector<LexError> errors_;

  // Lexer Handler does dispatching
  using LexHandler = Token (Lexer::*)(std::string_view leading_trivia);
  std::array<LexHandler, 256> dispatch_;

  // while not at end and a particular condition
  template <typename F> void consume_while(F &&predicate) {
    while (!is_at_end() && predicate(peek())) {
      advance();
    }
  }

  // --- Anchor Helper ---
  struct Anchor {
    Lexer &lex;
    std::size_t start_offset;
    SourceLocation start_loc;
    std::string_view trivia;

    std::string_view text() const {
      return lex.source_.substr(start_offset, lex.cursor_ - start_offset);
    }

    Token complete(TokenKind kind) const {
      return Token{kind, start_loc, text(), trivia};
    }
  };

  /// sets anchor and captures trivia
  Anchor anchor(std::string_view trivia) {
    return {*this, cursor_, current_loc(), trivia};
  }

  // is_at_end || != expected ;; calls advance()
  bool expect(char expected) {
    if (is_at_end() || peek() != expected) {
      return false;
    }
    advance();
    return true;
  }

  bool is_at_end() const { return cursor_ >= source_.size(); }

  char peek(std::size_t offset = 0) const {
    if (cursor_ + offset >= source_.size())
      return '\0';
    return source_[cursor_ + offset];
  }

  char advance() {
    char c = source_[cursor_++];
    if (c == '\n') {
      line_++;
      col_ = 1;
    } else {
      col_++;
    }
    return c;
  }

  static constexpr bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
  }

  static constexpr bool is_digit(char c) { return c >= '0' && c <= '9'; }

  static constexpr bool is_wspace(char c) {
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
  }

  SourceLocation current_loc() const {
    return {line_, col_, static_cast<uint32_t>(cursor_)};
  }

  void lex_block_comment() {
    SourceLocation start = current_loc();
    advance(); // '-'
    advance(); // '*'

    while (!is_at_end()) {
      if (peek() == '*' && peek(1) == '-') {
        advance(); // '*'
        advance(); // '-'
        return;    // Success
      }
      advance();
    }
    errors_.push_back({LexError::Kind::UnterminatedBlockComment, start});
  }

  std::string_view lex_trivia() {
    auto m = anchor("");

    while (!is_at_end()) {
      char c = peek();
      if (is_wspace(c)) {
        consume_while([](char c) { return is_wspace(c); });
        continue;
      }
      if (c == '-' && peek(1) == '-' && peek(2) == '-') {
        consume_while([](char c) { return c != '\n'; });
        continue;
      }
      if (c == '-' && peek(1) == '-') {
        consume_while([](char c) { return c != '\n'; });
        continue;
      }
      if (c == '-' && peek(1) == '*') {
        lex_block_comment();
        continue;
      }
      break;
    }
    return m.text();
  }

  static constexpr std::array<TokenKind, 256> char_to_kind = [] {
    std::array<TokenKind, 256> table{};
    table.fill(TokenKind::ILLEGAL);
    table['('] = TokenKind::OPAREN;
    table[')'] = TokenKind::CPAREN;
    table['{'] = TokenKind::OBRACE;
    table['}'] = TokenKind::CBRACE;
    table['['] = TokenKind::OBRACK;
    table[']'] = TokenKind::CBRACK;

    table[';'] = TokenKind::SEMI;
    table[','] = TokenKind::COMMA;
    table['#'] = TokenKind::HASH;
    table['@'] = TokenKind::AT;
    table['?'] = TokenKind::QUESTION;
    table['_'] = TokenKind::UND;
    table['~'] = TokenKind::TILDE;
    return table;
  }();

  Token lex_ident(std::string_view trivia) {
    auto m = anchor(trivia);
    consume_while(
        [](char c) { return is_alpha(c) || is_digit(c) || c == '_'; });

    std::string_view text = m.text();
    TokenKind kind = keyword_kind(text); // handles IDENT aswell
    return m.complete(kind);
  }

  Token lex_num(std::string_view trivia) {
    auto m = anchor(trivia);
    consume_while([](char c) { return is_digit(c); });
    if (peek() == '.' && is_digit(peek(1))) {
      advance();
      consume_while([](char c) { return is_digit(c); });
      return m.complete(TokenKind::FLOAT);
    }
    return m.complete(TokenKind::INT);
  }

  Token lex_dot_variants(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    if (expect('.')) {
      if (expect('=')) {
        return m.complete(TokenKind::DDOTEQ);
      }
      return m.complete(TokenKind::DDOT);
    }
    return m.complete(TokenKind::DOT);
  }

  Token lex_illegal(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    return m.complete(TokenKind::ILLEGAL);
  }

  Token lex_single(std::string_view trivia) {
    auto m = anchor(trivia);
    TokenKind kind = char_to_kind[static_cast<unsigned char>(advance())];
    if (kind == TokenKind::ILLEGAL)
      errors_.push_back({LexError::Kind::InvalidCharacter, m.start_loc});
    return m.complete(kind);
  }

  // -  ->  -=
  Token lex_minus_variants(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    if (expect('>'))
      return m.complete(TokenKind::ARROW);
    if (expect('='))
      return m.complete(TokenKind::MINUSEQ);

    return m.complete(TokenKind::MINUS);
  }

  // +  +=
  Token lex_add_variants(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    if (expect('='))
      return m.complete(TokenKind::PLUSEQ);

    return m.complete(TokenKind::PLUS);
  }

  // *  *=
  Token lex_star_variants(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    if (expect('='))
      return m.complete(TokenKind::STAREQ);

    return m.complete(TokenKind::STAR);
  }

  // /  /=
  Token lex_slash_variants(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    if (expect('='))
      return m.complete(TokenKind::SLASHEQ);

    return m.complete(TokenKind::SLASH);
  }

  // %  %=
  Token lex_modulo_variants(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    if (expect('='))
      return m.complete(TokenKind::MODULOEQ);

    return m.complete(TokenKind::MODULO);
  }

  /// =  ==  =>
  Token lex_eq_variants(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    if (expect('='))
      return m.complete(TokenKind::EQ);
    if (expect('>'))
      return m.complete(TokenKind::FATARROW);

    return m.complete(TokenKind::ASSIGN);
  }

  // !  !=
  Token lex_bang_variants(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    if (expect('='))
      return m.complete(TokenKind::NEQ);

    return m.complete(TokenKind::BANG);
  }

  // <  <=
  Token lex_lt_variants(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    if (expect('='))
      return m.complete(TokenKind::LTEQ);

    return m.complete(TokenKind::LT);
  }

  // >  >= >>
  Token lex_gt_variants(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    if (expect('='))
      return m.complete(TokenKind::GTEQ);
    if (expect('>'))
      return m.complete(TokenKind::PIPELINE);

    return m.complete(TokenKind::GT);
  }

  // :  ::
  Token lex_col_variants(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    if (expect(':'))
      return m.complete(TokenKind::CCOL);

    return m.complete(TokenKind::COL);
  }

  // &  &&
  Token lex_amp_variants(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    if (expect('&'))
      return m.complete(TokenKind::AND);

    return m.complete(TokenKind::AMP);
  }

  Token lex_pipe_variants(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    if (expect('|'))
      return m.complete(TokenKind::OR);

    return m.complete(TokenKind::PIPE);
  }

  Token lex_string(std::string_view trivia) {
    auto m = anchor(trivia);
    SourceLocation start = current_loc();
    advance(); // Consume opening "
    while (!is_at_end() && peek() != '"') {
      if (peek() == '\\') {
        advance(); // Consume '\'
        if (!is_at_end())
          advance(); // Consume escaped char
      } else {
        advance();
      }
    }

    if (is_at_end()) {
      errors_.push_back({LexError::Kind::UnterminatedString, start});
      return m.complete(TokenKind::ERROR);
    }

    advance(); // Consume closing "
    return m.complete(TokenKind::STRING);
  }
  void initialise_dispatch_table() {
    dispatch_.fill(&Lexer::lex_illegal);

    dispatch_['('] = &Lexer::lex_single;
    dispatch_[')'] = &Lexer::lex_single;
    dispatch_['['] = &Lexer::lex_single;
    dispatch_[']'] = &Lexer::lex_single;
    dispatch_['{'] = &Lexer::lex_single;
    dispatch_['}'] = &Lexer::lex_single;
    dispatch_[';'] = &Lexer::lex_single;
    dispatch_[','] = &Lexer::lex_single;
    dispatch_['@'] = &Lexer::lex_single;
    dispatch_['?'] = &Lexer::lex_single;
    dispatch_['#'] = &Lexer::lex_single;
    dispatch_['~'] = &Lexer::lex_single;

    dispatch_['_'] = &Lexer::lex_ident;
    for (int c = 'a'; c <= 'z'; ++c)
      dispatch_[static_cast<unsigned char>(c)] = &Lexer::lex_ident;
    for (int c = 'A'; c <= 'Z'; ++c)
      dispatch_[static_cast<unsigned char>(c)] = &Lexer::lex_ident;

    for (int c = '0'; c <= '9'; c++)
      dispatch_[static_cast<unsigned char>(c)] = &Lexer::lex_num;

    dispatch_['.'] = &Lexer::lex_dot_variants;
    dispatch_['-'] = &Lexer::lex_minus_variants;
    dispatch_['+'] = &Lexer::lex_add_variants;
    dispatch_['*'] = &Lexer::lex_star_variants;
    dispatch_['/'] = &Lexer::lex_slash_variants;
    dispatch_['%'] = &Lexer::lex_modulo_variants;
    dispatch_['='] = &Lexer::lex_eq_variants;
    dispatch_['!'] = &Lexer::lex_bang_variants;
    dispatch_['<'] = &Lexer::lex_lt_variants;
    dispatch_['>'] = &Lexer::lex_gt_variants;
    dispatch_[':'] = &Lexer::lex_col_variants;
    dispatch_['"'] = &Lexer::lex_string;
    dispatch_['&'] = &Lexer::lex_amp_variants;
    dispatch_['|'] = &Lexer::lex_pipe_variants;
  }
};

} // namespace vd
