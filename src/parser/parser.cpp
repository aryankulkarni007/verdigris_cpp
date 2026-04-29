#include "compiler/parser.hpp"
#include "compiler/token.hpp"
#include "compiler/tree_builder.hpp"
#include <cstddef>

int vd::Parser::left_bp(vd::TokenKind k) {
  using namespace vd; // to avoid local hassle
  switch (k) {
  // level 1 - assignment (right-assoc, handled in parse_expr)
  case TokenKind::ASSIGN:
  case TokenKind::PLUSEQ:
  case TokenKind::MINUSEQ:
  case TokenKind::STAREQ:
  case TokenKind::SLASHEQ:
  case TokenKind::MODULOEQ:
    return 10;

    // level 2 - pipeline
  case TokenKind::PIPELINE:
    return 20;

    // level 3 - logical or
  case TokenKind::OR:
    return 30;

    // level 4 - logical and
  case TokenKind::AND:
    return 40;

    // level 5 - equality
  case TokenKind::EQ:
  case TokenKind::NEQ:
    return 50;

    // level 6 - comparison
  case TokenKind::LT:
  case TokenKind::GT:
  case TokenKind::LTEQ:
  case TokenKind::GTEQ:
    return 60;

    // level 7 - range
  case TokenKind::DDOT:
  case TokenKind::DDOTEQ:
    return 70;

    // level 8 - additive
  case TokenKind::PLUS:
  case TokenKind::MINUS:
    return 80;

    // level 9 - multiplicative
  case TokenKind::STAR:
  case TokenKind::SLASH:
  case TokenKind::MODULO:
    return 90;

    // level 10 - cast
  case TokenKind::AS:
    return 100;

    // level 12 - postfix error prop
  case TokenKind::BANG:
    return 120;

    // level 13 - call / index / field
  case TokenKind::OPAREN:
  case TokenKind::OBRACK:
  case TokenKind::DOT:
    return 130;

  default:
    return 0;
  }
}

void vd::Parser::parse_primary() {
  switch (peek()) {
  // literals
  case TokenKind::INT:
  case TokenKind::FLOAT:
  case TokenKind::STRING:
  case TokenKind::TRUE:
  case TokenKind::FALSE:
  case TokenKind::_NULL: {
    auto m = open();
    bump();
    close(m, SyntaxKind::LITERAL_E);
    break;
  }
  // identifier
  case TokenKind::IDENT: {
    auto m = open();
    bump();
    close(m, SyntaxKind::IDENT_E);
    break;
  }
  // grouped: ( expr )
  case TokenKind::OPAREN: {
    auto m = open();
    bump(); // (
    parse_expr(0);
    expect(TokenKind::CPAREN);
    close(m, SyntaxKind::PAREN_E);
    break;
  }
  // prefix unary: - ! & *
  case TokenKind::MINUS:
  case TokenKind::BANG:
  case TokenKind::AMP:
  case TokenKind::STAR: {
    auto m = open();
    bump();
    parse_expr(110);
    close(m, SyntaxKind::UNARY_E);
    break;
  }
  default:
    // error_recovery("expected expression");
    break;
  }
}

void vd::Parser::parse_arg_list() {
  auto m = open();
  expect(TokenKind::OPAREN); // (
  while (peek() != TokenKind::CPAREN && peek() != TokenKind::_EOF) {
    parse_expr(0);
    if (!eat(TokenKind::COMMA))
      break;
  }
  expect(TokenKind::CPAREN); // )
  close(m, SyntaxKind::ARG_LIST);
}

void vd::Parser::parse_expr(int min_bp) {
  // -- NUD: parse prefix/primary --
  long cp = static_cast<long>(checkpoint());

  parse_primary();

  // -- LED loop --
  while (true) {
    TokenKind op = peek();
    int lbp = left_bp(op);
    if (lbp <= min_bp)
      break;

    // postfix !  — unary, no right side
    if (op == TokenKind::BANG) {
      events_.insert(events_.begin() + cp,
                     Event{EventKind::OPEN, SyntaxKind::UNARY_E});
      bump();
      events_.push_back(Event{EventKind::CLOSE, SyntaxKind::UNARY_E});
      cp = static_cast<long>(checkpoint());
      continue;
    }

    // call ( )
    if (op == TokenKind::OPAREN) {
      events_.insert(events_.begin() + cp,
                     Event{EventKind::OPEN, SyntaxKind::CALL_E});
      // parse_arg_list();
      events_.push_back(Event{EventKind::CLOSE, SyntaxKind::CALL_E});
      cp = static_cast<long>(checkpoint());
      continue;
    }

    // field access .
    if (op == TokenKind::DOT) {
      events_.insert(events_.begin() + cp,
                     Event{EventKind::OPEN, SyntaxKind::FIELD_E});
      bump(); // .
      expect(TokenKind::IDENT);
      events_.push_back(Event{EventKind::CLOSE, SyntaxKind::FIELD_E});
      cp = static_cast<long>(checkpoint());
      continue;
    }

    // index [ ]
    if (op == TokenKind::OBRACK) {
      events_.insert(events_.begin() + cp,
                     Event{EventKind::OPEN, SyntaxKind::INDEX_E});
      bump(); // [
      parse_expr(0);
      expect(TokenKind::CBRACK);
      events_.push_back(Event{EventKind::CLOSE, SyntaxKind::INDEX_E});
      cp = static_cast<long>(checkpoint());
      continue;
    }

    // generic binary
    events_.insert(events_.begin() + cp,
                   Event{EventKind::OPEN, SyntaxKind::BINARY_E});
    bump();
    parse_expr(is_right_assoc(op) ? lbp - 1 : lbp);
    events_.push_back(Event{EventKind::CLOSE, SyntaxKind::BINARY_E});
    cp = static_cast<long>(checkpoint());
  }
}

void vd::Parser::raw_dump() const {
  std::printf("--------------------------------------------\n");
  for (size_t i = 0; i < events_.size(); ++i) {
    const auto &e = events_[i];
    std::printf("[%03zu] Kind: %d | Syntax: %d\n", i, static_cast<int>(e.kind),
                static_cast<int>(e.syntax_kind));
  }
  std::printf("--------------------------------------------\n");
}
