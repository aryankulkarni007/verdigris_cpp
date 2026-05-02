#pragma once

#include "token.hpp"
#include <array>
#include <cstddef>
#include <string_view>

#include <string_view>
#include <unordered_map>

namespace vd {
inline TokenKind keyword_kind(std::string_view s) {
  using namespace std::string_view_literals;
  static const std::unordered_map<std::string_view, TokenKind> keywords = {
      // clang-format off
        {"as", TokenKind::AS},           {"let", TokenKind::LET},
        {"mut", TokenKind::MUT},         {"structure", TokenKind::STRUCTURE},
        {"type", TokenKind::TYPE},       {"for", TokenKind::FOR},
        {"in", TokenKind::IN},           {"match", TokenKind::MATCH},
        {"loop", TokenKind::LOOP},       {"while", TokenKind::WHILE},
        {"break", TokenKind::BREAK},     {"continue", TokenKind::CONTINUE},
        {"if", TokenKind::IF},           {"else", TokenKind::ELSE},
        {"return", TokenKind::RETURN},   {"null", TokenKind::_NULL},
        {"self", TokenKind::SELF},       {"export", TokenKind::EXPORT},
        {"import", TokenKind::IMPORT},   {"interface", TokenKind::INTERFACE},
        {"implement", TokenKind::IMPLEMENT}, {"effect", TokenKind::EFFECT}
    };
  // clang-format on

  if (auto it = keywords.find(s); it != keywords.end())
    return it->second;
  return TokenKind::IDENT;
}

class Lexer {
public:
  explicit Lexer(std::string_view source) : source_(source) {}

  Token next_token() {
    std::string_view leading_trivia = lex_trivia();
    if (is_at_end())
      return anchor(leading_trivia).complete(TokenKind::_EOF);

    auto handler = dispatch_table[static_cast<unsigned char>(peek())];
    return (this->*handler)(leading_trivia);
  }

private:
  // --- DATA ---
  struct LexError {
    enum class Kind {
      UnterminatedBlockComment,
      UnterminatedString,
      UnterminatedChar,
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
        consume_while([](char ch) { return is_wspace(ch); });
        continue;
      }
      if (c == '-' && peek(1) == '-' && peek(2) == '-') {
        consume_while([](char ch) { return ch != '\n'; });
        continue;
      }
      if (c == '-' && peek(1) == '-') {
        consume_while([](char ch) { return ch != '\n'; });
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

    for (int i = 0; i < 256; ++i) {
      table[static_cast<std::size_t>(i)] = TokenKind::ILLEGAL;
    }

    table[static_cast<unsigned char>('(')] = TokenKind::OPAREN;
    table[static_cast<unsigned char>(')')] = TokenKind::CPAREN;
    table[static_cast<unsigned char>('{')] = TokenKind::OBRACE;
    table[static_cast<unsigned char>('}')] = TokenKind::CBRACE;
    table[static_cast<unsigned char>('[')] = TokenKind::OBRACK;
    table[static_cast<unsigned char>(']')] = TokenKind::CBRACK;
    table[static_cast<unsigned char>(';')] = TokenKind::SEMI;
    table[static_cast<unsigned char>(',')] = TokenKind::COMMA;
    table[static_cast<unsigned char>('#')] = TokenKind::HASH;
    table[static_cast<unsigned char>('@')] = TokenKind::AT;
    table[static_cast<unsigned char>('?')] = TokenKind::QUESTION;
    table[static_cast<unsigned char>('_')] = TokenKind::UND;
    table[static_cast<unsigned char>('~')] = TokenKind::TILDE;

    return table;
  }();

  // TODO: needs to differentiate between _ and ident that starts with _
  Token lex_ident(std::string_view trivia) {
    auto m = anchor(trivia);
    consume_while(
        [](char ch) { return is_alpha(ch) || is_digit(ch) || ch == '_'; });

    std::string_view text = m.text();
    if (text == "_")
      return m.complete(TokenKind::UND);
    TokenKind kind = keyword_kind(text); // handles IDENT aswell
    return m.complete(kind);
  }

  Token lex_num(std::string_view trivia) {
    auto m = anchor(trivia);
    consume_while([](char ch) { return is_digit(ch); });
    if (peek() == '.' && is_digit(peek(1))) {
      advance();
      consume_while([](char ch) { return is_digit(ch); });
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

  // ^  ^=
  Token lex_carent_variants(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    if (expect('='))
      return m.complete(TokenKind::CARETEQ);

    return m.complete(TokenKind::CARET);
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

  // <  <=  <<
  Token lex_lt_variants(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    if (expect('='))
      return m.complete(TokenKind::LTEQ);
    if (expect('<'))
      return m.complete(TokenKind::SHL);

    return m.complete(TokenKind::LT);
  }

  // >  >= >>
  Token lex_gt_variants(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    if (expect('='))
      return m.complete(TokenKind::GTEQ);
    if (expect('>'))
      return m.complete(TokenKind::SHR);

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

  // |  ||  |>
  Token lex_pipe_variants(std::string_view trivia) {
    auto m = anchor(trivia);
    advance();
    if (expect('|'))
      return m.complete(TokenKind::OR);
    if (expect('>'))
      return m.complete(TokenKind::PIPELINE);

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

  Token lex_char(std::string_view trivia) {
    auto m = anchor(trivia);
    SourceLocation start = current_loc();
    advance(); // Consume opening '
    if (is_at_end()) {
      errors_.push_back({LexError::Kind::UnterminatedChar, start});
      return m.complete(TokenKind::ERROR);
    }
    if (peek() == '\\') {
      advance(); // Consume '\'
      if (is_at_end()) {
        errors_.push_back({LexError::Kind::UnterminatedChar, start});
        return m.complete(TokenKind::ERROR);
      }
      advance(); // Consume escaped char
    } else {
      advance(); // Consume the character
    }
    if (is_at_end() || peek() != '\'') {
      errors_.push_back({LexError::Kind::UnterminatedChar, start});
      return m.complete(TokenKind::ERROR);
    }
    advance(); // Consume closing '
    return m.complete(TokenKind::CHAR);
  }

  static constexpr auto dispatch_table = [] {
    std::array<LexHandler, 256> table{};

    for (int i = 0; i < 256; ++i)
      table[static_cast<std::size_t>(i)] = &Lexer::lex_illegal;

    // helper to fill ranges
    auto set_range = [&](char start, char end, LexHandler handler) {
      for (int c = static_cast<unsigned char>(start);
           c <= static_cast<unsigned char>(end); ++c) {
        table[static_cast<std::size_t>(c)] = handler;
      }
    };

    // Identifiers and Numbers
    set_range('a', 'z', &Lexer::lex_ident);
    set_range('A', 'Z', &Lexer::lex_ident);
    set_range('0', '9', &Lexer::lex_num);
    table['_'] = &Lexer::lex_ident;

    // Single character tokens
    table['('] = &Lexer::lex_single;
    table[')'] = &Lexer::lex_single;
    table['['] = &Lexer::lex_single;
    table[']'] = &Lexer::lex_single;
    table['{'] = &Lexer::lex_single;
    table['}'] = &Lexer::lex_single;
    table[';'] = &Lexer::lex_single;
    table[','] = &Lexer::lex_single;
    table['@'] = &Lexer::lex_single;
    table['?'] = &Lexer::lex_single;
    table['#'] = &Lexer::lex_single;
    table['~'] = &Lexer::lex_single;

    // Multi-character variants
    table['.'] = &Lexer::lex_dot_variants;
    table['-'] = &Lexer::lex_minus_variants;
    table['+'] = &Lexer::lex_add_variants;
    table['*'] = &Lexer::lex_star_variants;
    table['/'] = &Lexer::lex_slash_variants;
    table['%'] = &Lexer::lex_modulo_variants;
    table['='] = &Lexer::lex_eq_variants;
    table['!'] = &Lexer::lex_bang_variants;
    table['<'] = &Lexer::lex_lt_variants;
    table['>'] = &Lexer::lex_gt_variants;
    table[':'] = &Lexer::lex_col_variants;
    table['&'] = &Lexer::lex_amp_variants;
    table['|'] = &Lexer::lex_pipe_variants;
    table['^'] = &Lexer::lex_carent_variants;

    // Literals
    table['"'] = &Lexer::lex_string;
    table['\''] = &Lexer::lex_char;

    return table;
  }();
};

} // namespace vd
