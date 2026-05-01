#include "compiler/parser.hpp"
#include "compiler/token.hpp"
#include "compiler/tree_builder.hpp"
#include <cstddef>
#include <iomanip>

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

void vd::Parser::parse_primary(bool allow_struct_lit) {
  switch (peek()) {
  // literals
  case TokenKind::INT:
  case TokenKind::FLOAT:
  case TokenKind::STRING:
  case TokenKind::CHAR:
  case TokenKind::TRUE:
  case TokenKind::FALSE:
  case TokenKind::_NULL: {
    auto m = open();
    bump();
    m.set_kind(SyntaxKind::LITERAL_E);
    break;
  }
    // identifier
  case TokenKind::IDENT: {
    // bool needed because parser thought a for loop was a struct_lit
    if (allow_struct_lit && is_struct_lit()) {
      parse_struct_lit();
    } else {
      auto m = open();
      bump();
      m.set_kind(SyntaxKind::IDENT_E);
    }
    break;
  }
    // grouped: ( expr ) or tuple expr
  case TokenKind::OPAREN: {
    auto m = open();
    bump(); // (
    parse_expr(0);
    if (eat(TokenKind::COMMA)) {
      while (peek() != TokenKind::CPAREN && peek() != TokenKind::_EOF) {
        parse_expr(0);
        if (!eat(TokenKind::COMMA))
          break;
      }
      expect(TokenKind::CPAREN);
      m.set_kind(SyntaxKind::TUPLE_E);
    } else {
      expect(TokenKind::CPAREN);
      m.set_kind(SyntaxKind::PAREN_E);
    }
    break;
  }
    // NOTE: PARSE_ARRAY_LITERAL
  case TokenKind::OBRACK: {
    auto m = open();
    bump(); // [
    while (peek() != TokenKind::CBRACK && peek() != TokenKind::_EOF) {
      parse_expr(0);
      if (!eat(TokenKind::COMMA))
        break;
    }
    expect(TokenKind::CBRACK);
    m.set_kind(SyntaxKind::ARRAY_LIT);
    break;
  }
  // prefix unary: - ! & *
  case TokenKind::MINUS:
  case TokenKind::BANG:
  case TokenKind::AMP:
  case TokenKind::STAR: {
    auto m = open();
    bump(); // - ! & or *
    parse_expr(110);
    m.set_kind(SyntaxKind::UNARY_E);
    break;
  }
  default:
    error_recover("unexpected token in expression");
    break;
  }
}

void vd::Parser::parse_expr(int min_bp, bool allow_struct_lit) {
  long cp = static_cast<long>(checkpoint());
  parse_primary(allow_struct_lit);

  while (true) {
    TokenKind op = peek();
    int lbp = left_bp(op);
    if (lbp <= min_bp)
      break;

    if (op == TokenKind::BANG) {
      long new_cp = static_cast<long>(cp); // save before insert
      events_.insert(events_.begin() + cp,
                     Event{EventKind::OPEN, SyntaxKind::UNARY_E});
      bump();
      events_.push_back(Event{EventKind::CLOSE, SyntaxKind::UNARY_E});
      cp = new_cp; // point to the OPEN
      continue;
    }

    if (op == TokenKind::OPAREN) {
      long new_cp = cp;
      events_.insert(events_.begin() + cp,
                     Event{EventKind::OPEN, SyntaxKind::CALL_E});
      parse_args();
      events_.push_back(Event{EventKind::CLOSE, SyntaxKind::CALL_E});
      cp = new_cp;
      continue;
    }

    if (op == TokenKind::DOT) {
      long new_cp = cp;
      events_.insert(events_.begin() + cp,
                     Event{EventKind::OPEN, SyntaxKind::FIELD_E});
      bump();
      expect(TokenKind::IDENT);
      events_.push_back(Event{EventKind::CLOSE, SyntaxKind::FIELD_E});
      cp = new_cp;
      continue;
    }

    if (op == TokenKind::OBRACK) {
      long new_cp = cp;
      events_.insert(events_.begin() + cp,
                     Event{EventKind::OPEN, SyntaxKind::INDEX_E});
      bump();
      parse_expr(0);
      expect(TokenKind::CBRACK);
      events_.push_back(Event{EventKind::CLOSE, SyntaxKind::INDEX_E});
      cp = new_cp;
      continue;
    }

    // binary
    long new_cp = cp;
    events_.insert(events_.begin() + cp,
                   Event{EventKind::OPEN, SyntaxKind::BINARY_E});
    bump();
    parse_expr(is_right_assoc(op) ? lbp - 1 : lbp);
    events_.push_back(Event{EventKind::CLOSE, SyntaxKind::BINARY_E});
    cp = new_cp;
  }
}

void vd::Parser::parse_type_ref() {
  std::size_t cp = checkpoint();
  auto m = open();

  if (eat(TokenKind::AMP)) {
    // &[type]
    expect(TokenKind::OBRACK);
    parse_type_ref();
    expect(TokenKind::CBRACK);
    m.set_kind(SyntaxKind::SLICE_TYPE_REF);
  } else if (peek() == TokenKind::OBRACK) {
    // [type] or [type; size]
    bump();
    parse_type_ref();
    if (eat(TokenKind::SEMI))
      parse_expr();
    expect(TokenKind::CBRACK);
    m.set_kind(SyntaxKind::ARRAY_TYPE_REF);
  } else if (peek() == TokenKind::OPAREN && peek(1) == TokenKind::CPAREN) {
    // () - the unit return type
    bump(); // (
    bump(); // )
    m.set_kind(SyntaxKind::VOID);
  } else {
    // any named type: int, float, Point, T, ...
    if (!expect(TokenKind::IDENT)) {
      error_recover("expected type name");
      return;
    }
    if (peek() == TokenKind::OBRACK)
      parse_gen_args(); // Vec[int]
    m.set_kind(SyntaxKind::TYPE_REF);
  }

  // postfix
  while (true) {
    if (peek() == TokenKind::QUESTION) {
      events_.insert(events_.begin() + static_cast<long>(cp),
                     Event{EventKind::OPEN, SyntaxKind::OPTIONAL_TYPE_REF});
      bump();
      events_.push_back(Event{EventKind::CLOSE, SyntaxKind::OPTIONAL_TYPE_REF});
      cp = checkpoint();
    } else if (peek() == TokenKind::PIPE) {
      events_.insert(events_.begin() + static_cast<long>(cp),
                     Event{EventKind::OPEN, SyntaxKind::UNION_TYPE_REF});
      bump();
      parse_type_ref();
      events_.push_back(Event{EventKind::CLOSE, SyntaxKind::UNION_TYPE_REF});
      cp = checkpoint();
    } else {
      break;
    }
  }
}

// NOTE: these following two functions replace the parse_typed_binding, and
// parse_let_stmt and parse_mut_statement

void vd::Parser::parse_typed_binding() {
  auto m = open();
  [[maybe_unused]] bool is_mut = eat(TokenKind::MUT);
  parse_type_ref();         // Vec[int] or int or [int; 4]
  expect(TokenKind::IDENT); // var name
  if (eat(TokenKind::ASSIGN)) {
    parse_expr(); // array
  } else if (peek() == TokenKind::OBRACE) {
    parse_struct_lit(); // {} or { .x: 1, .y: 2 }
  } else {
    error_recover("expected '=' or '{' in binding");
    return;
  }
  expect(TokenKind::SEMI);
  m.set_kind(SyntaxKind::TYPED_BINDING);
}

void vd::Parser::parse_inferred_binding() {
  auto m = open();
  bump(); // let or mut
  parse_pattern();
  expect(TokenKind::ASSIGN);
  parse_expr();
  expect(TokenKind::SEMI);
  m.set_kind(SyntaxKind::INFERRED_BINDING);
}

// NOTE: ----------------------------------

void vd::Parser::parse_return_stmt() {
  auto m = open();
  expect(TokenKind::RETURN);
  if (peek() != TokenKind::SEMI)
    parse_expr();
  expect(TokenKind::SEMI);
  m.set_kind(SyntaxKind::RETURN_S);
}

void vd::Parser::parse_block() {
  auto m = open();
  if (!expect(TokenKind::OBRACE)) {
    error_recover("expected '{'");
    return;
  }

  while (peek() != TokenKind::CBRACE && peek() != TokenKind::_EOF) {
    bool terminated = parse_stmt();
    if (!terminated) {
      // parse_return_stmt();
      break;
    }
  }

  if (!expect(TokenKind::CBRACE)) {
    error_recover("expected '}'");
    return;
  }
  m.set_kind(SyntaxKind::BLOCK_E);
}

/// true if stmt was terminated (had ;)
/// false if expr had no ; = implicit return candidate
bool vd::Parser::parse_stmt() {
  switch (peek()) {
  // variable bindings
  case TokenKind::LET:
    parse_inferred_binding();
    return true;
  case TokenKind::MUT:
    if (is_typed_binding())
      parse_typed_binding();
    else
      parse_inferred_binding();
    return true; // always terminated with ';'

  // explicit return
  case TokenKind::RETURN:
    parse_return_stmt();
    return true; // always terminated with ';'

    // Control flow
  case TokenKind::IF: {
    auto m = open();
    parse_if_expr();
    if (eat(TokenKind::SEMI)) {
      m.set_kind(SyntaxKind::EXPR_S);
      return true;
    } else {
      m.set_kind(SyntaxKind::IMPLICIT_RETURN_E);
      return false;
    }
  }
  case TokenKind::MATCH: {
    auto m = open();
    parse_match_expr();
    if (eat(TokenKind::SEMI)) {
      m.set_kind(SyntaxKind::EXPR_S);
      return true;
    } else {
      m.set_kind(SyntaxKind::IMPLICIT_RETURN_E);
      return false;
    }
  }
  case TokenKind::FOR:
    parse_for_stmt();
    return true;
  case TokenKind::WHILE:
    parse_while_stmt();
    return true;
  case TokenKind::LOOP:
    parse_loop_stmt();
    return true;
  case TokenKind::BREAK:
    parse_break_stmt();
    return true;
  case TokenKind::CONTINUE:
    parse_continue_stmt();
    return true;

  case TokenKind::OBRACE:
    parse_block();
    return false;

    // expression statement (any token that can start an expression)
  case TokenKind::IDENT: {
    if (is_fn_def()) {
      parse_fn();
      return true;
    }
    if (is_typed_binding()) {
      parse_typed_binding();
      return true;
    }
    // expression statement
    auto m = open();
    parse_expr(0);
    if (eat(TokenKind::SEMI)) {
      m.set_kind(SyntaxKind::EXPR_S);
      return true;
    } else {
      m.set_kind(SyntaxKind::IMPLICIT_RETURN_E);
      return false;
    }
  }
  case TokenKind::STRUCTURE:
    parse_structure();
    return true;
  case TokenKind::IMPLEMENT:
    parse_implement();
    return true;
  case TokenKind::TYPE:
    parse_type_def();
    return true;
  case TokenKind::INTERFACE:
    parse_interface();
    return true;
  case TokenKind::OBRACK: {
    // array type: [int; 4] fixed = ...
    // array literal: [1, 2, 3]
    if (is_typed_binding()) {
      parse_typed_binding();
      return true;
    }
    // fall through to expression
    auto m = open();
    parse_expr(0);
    if (eat(TokenKind::SEMI)) {
      m.set_kind(SyntaxKind::EXPR_S);
      return true;
    } else {
      m.set_kind(SyntaxKind::IMPLICIT_RETURN_E);
      return false;
    }
  }
  case TokenKind::AMP: {
    // slice dispatch
    if (is_typed_binding()) {
      parse_typed_binding();
      return true;
    }
    // unary expression dispatch
    auto m = open();
    parse_expr(0);
    if (eat(TokenKind::SEMI)) {
      m.set_kind(SyntaxKind::EXPR_S);
      return true;
    } else {
      m.set_kind(SyntaxKind::IMPLICIT_RETURN_E);
      return false;
    }
  }
  case TokenKind::OPAREN: {
    // tuple destructuring binding
    if (is_typed_binding()) {
      parse_typed_binding();
      return true;
    }
    // expression starting with (
    auto m = open();
    parse_expr(0);
    if (eat(TokenKind::SEMI)) {
      m.set_kind(SyntaxKind::EXPR_S);
      return true;
    } else {
      m.set_kind(SyntaxKind::IMPLICIT_RETURN_E);
      return false;
    }
  }
    // fall through to expression
  case TokenKind::INT:
  case TokenKind::FLOAT:
  case TokenKind::STRING:
  case TokenKind::CHAR:
  case TokenKind::TRUE:
  case TokenKind::FALSE:
  case TokenKind::_NULL:
  case TokenKind::STAR:
  case TokenKind::MINUS:
    // case TokenKind::PIPE: { // lambda |x| -> ...
  case TokenKind::BANG: {
    auto m = open();
    parse_expr(0);
    if (eat(TokenKind::SEMI)) {
      m.set_kind(SyntaxKind::EXPR_S);
      return true;
    } else {
      m.set_kind(SyntaxKind::IMPLICIT_RETURN_E);
      return false; // implicit return
    }
  }

  default:
    error_recover("unexpected token in statement");
    return true;
  }
}

// NOTE: owns the parens
void vd::Parser::parse_params() {
  auto m = open();
  std::cerr << "parse_params: peek=" << token_name(peek()) << "\n";
  if (!expect(TokenKind::OPAREN)) {
    error_recover("expected '(' after function name");
    return;
  }
  while (peek() != TokenKind::CPAREN && peek() != TokenKind::_EOF) {
    if (peek() == TokenKind::SELF) {
      auto pm = open();
      bump();
      pm.set_kind(SyntaxKind::SELF_PARAM);
    } else {
      auto pm = open();
      parse_type_ref();
      expect(TokenKind::IDENT);
      pm.set_kind(SyntaxKind::PARAM);
    }
    if (!eat(TokenKind::COMMA))
      break;
  }
  if (!expect(TokenKind::CPAREN)) {
    error_recover("expected ')' after function args");
    return;
  }
  m.set_kind(SyntaxKind::PARAM_LIST);
}

void vd::Parser::parse_args() {
  auto m = open();
  expect(TokenKind::OPAREN); // (
  while (peek() != TokenKind::CPAREN && peek() != TokenKind::_EOF) {
    parse_expr(); // type_name name
    if (!eat(TokenKind::COMMA))
      break;
  }
  expect(TokenKind::CPAREN); // )
  m.set_kind(SyntaxKind::ARG_LIST);
}

// CALL/USE SITE (gen args):
// Vec[int]           -- int is a concrete type, no constraints
// Map[string, User]  -- two concrete types
// foo[Point](x)      -- explicit type arg
void vd::Parser::parse_gen_args() {
  auto m = open();
  expect(TokenKind::OBRACK);
  while (peek() != TokenKind::CBRACK && peek() != TokenKind::_EOF) {
    parse_type_ref();
    if (!eat(TokenKind::COMMA))
      break;
  }
  expect(TokenKind::CBRACK);
  m.set_kind(SyntaxKind::GEN_ARG_LIST);
}

// DEFINITION SITE (gen params):
// identity[T](value: T) -> T          -- T is a name, can have constraints
// get_or_insert[K: Hashable + Eq, V]  -- K has constraint
// interface Add[Rhs = Self]           -- Rhs has default
void vd::Parser::parse_gen_params() {
  auto m = open();
  expect(TokenKind::OBRACK);

  while (peek() != TokenKind::CBRACK && peek() != TokenKind::_EOF) {
    expect(TokenKind::IDENT); // param name: T, K, V, Rhs
    // optional constraint: : Hashable + Eq
    if (eat(TokenKind::COL)) {
      parse_type_ref(); // Hashable
      while (eat(TokenKind::PLUS))
        parse_type_ref(); // Eq
    }
    // optional default: = Self
    if (eat(TokenKind::ASSIGN))
      parse_type_ref(); // Self
    if (!eat(TokenKind::COMMA))
      break;
  }
  if (!expect(TokenKind::CBRACK)) {
    error_recover("expected ']' after generic args");
    return;
  }
  m.set_kind(SyntaxKind::GEN_PARAM_LIST);
}

void vd::Parser::parse_fn() {
  auto m = open();
  expect(TokenKind::IDENT);
  if (peek() == TokenKind::OBRACK)
    parse_gen_params();
  parse_params();            // handles if there are '('
  if (eat(TokenKind::ARROW)) // optional arrow if () return
    parse_type_ref();        // return type
  if (eat(TokenKind::SEMI))
    m.set_kind(SyntaxKind::FN_SIG);
  else { // start of fn def
    parse_block();
    m.set_kind(SyntaxKind::FN_D);
  }
}

void vd::Parser::parse_field() {
  auto m = open();
  [[maybe_unused]] bool is_export = eat(TokenKind::EXPORT);
  // i.e. no ident or col
  if (!expect(TokenKind::IDENT)) {
    error_recover("expected field name");
    return;
  }
  if (!expect(TokenKind::COL)) {
    error_recover("expected ':' after field name");
    return;
  }
  parse_type_ref();
  eat(TokenKind::COMMA);
  m.set_kind(SyntaxKind::FIELD_E);
}

void vd::Parser::parse_structure() {
  auto m = open();
  expect(TokenKind::STRUCTURE);
  expect(TokenKind::IDENT); // name
  if (peek() == TokenKind::OBRACK)
    parse_gen_params(); // [T, U]
  if (!expect(TokenKind::OBRACE)) {
    error_recover("expected '{' after structure def");
    return;
  }

  while (peek() != TokenKind::CBRACE && peek() != TokenKind::_EOF) {
    if (is_fn_def())
      parse_fn(); // method sig → ends with ; → FN_SIG
    else
      parse_field(); // field → name: type,
  }

  if (!expect(TokenKind::CBRACE)) {
    error_recover("expected '}' after structure body");
    return;
  }
  m.set_kind(SyntaxKind::STRUCTURE_D);
}

void vd::Parser::parse_implement() {
  auto m = open();
  expect(TokenKind::IMPLEMENT);
  expect(TokenKind::IDENT);
  if (!expect(TokenKind::OBRACE)) {
    error_recover("expected '{' in implement block");
    return;
  }
  while (peek() != TokenKind::CBRACE && peek() != TokenKind::_EOF)
    parse_fn(); // always full def here, never sig
  if (!expect(TokenKind::CBRACE)) {
    error_recover("expected '}' after implement block");
    return;
  }
  m.set_kind(SyntaxKind::IMPLEMENT_D);
}

void vd::Parser::parse_variant() {
  auto m = open();
  expect(TokenKind::IDENT);
  if (eat(TokenKind::OPAREN)) {
    while (peek() != TokenKind::CPAREN && peek() != TokenKind::_EOF) {
      parse_type_ref();
      if (!eat(TokenKind::COMMA))
        break;
    }
    expect(TokenKind::CPAREN);
  }
  m.set_kind(SyntaxKind::VARIANT_D);
}

void vd::Parser::parse_type_def() {
  auto m = open();
  expect(TokenKind::TYPE);
  expect(TokenKind::IDENT);
  if (peek() == TokenKind::OBRACK)
    parse_gen_params();
  expect(TokenKind::ASSIGN);
  eat(TokenKind::PIPE); // optional leading |
  parse_variant();
  while (eat(TokenKind::PIPE))
    parse_variant();
  m.set_kind(SyntaxKind::TYPE_D);
}

void vd::Parser::parse_interface() {
  auto m = open();
  expect(TokenKind::INTERFACE);
  expect(TokenKind::IDENT);
  if (peek() == TokenKind::OBRACK)
    parse_gen_params(); // handles the operator overload or generic case

  if (!expect(TokenKind::OBRACE)) {
    error_recover("expected '{' after interface start");
    return;
  }
  while (peek() != TokenKind::CBRACE && peek() != TokenKind::_EOF) {
    if (is_fn_def())
      parse_fn(); // method sig, must end with ;
    else {
      // interface inclusion: Hashable;
      if (!expect(TokenKind::IDENT)) {
        error_recover("expected method or interface name");
        continue; // skip to next member
      }
      expect(TokenKind::SEMI);
    }
  }
  if (!expect(TokenKind::CBRACE)) {
    error_recover("expected '}' after interface body");
    return;
  }
  m.set_kind(SyntaxKind::INTERFACE_D);
}

void vd::Parser::parse_if_expr() {
  auto m = open();
  expect(TokenKind::IF);
  parse_expr(0, false);
  parse_block();
  if (eat(TokenKind::ELSE)) {
    if (peek() == TokenKind::IF)
      parse_if_expr();
    else
      parse_block();
  }
  m.set_kind(SyntaxKind::IF_E);
}

// WILDCARD_P, BIND_P, TUPLE_P, ENUM_P, GUARD_P, OR_P,
void vd::Parser::parse_pattern() {
  auto m = open();

  switch (peek()) {
  // wildcard
  case TokenKind::UND:
    bump();
    m.set_kind(SyntaxKind::WILDCARD_P);
    break;

  // literal pattern: 0, true, "hello"
  case TokenKind::INT:
  case TokenKind::FLOAT:
  case TokenKind::STRING:
  case TokenKind::CHAR:
  case TokenKind::TRUE:
  case TokenKind::FALSE:
  case TokenKind::_NULL:
    bump();
    m.set_kind(SyntaxKind::LITERAL_P);
    break;

  //  (x, y)    circle(r)   rectangle(w, h))
  case TokenKind::OPAREN:
    bump();
    while (peek() != TokenKind::CPAREN && peek() != TokenKind::_EOF) {
      parse_pattern();
      if (!eat(TokenKind::COMMA))
        break;
    }
    expect(TokenKind::CPAREN);
    m.set_kind(SyntaxKind::TUPLE_P);
    break;

  // identifier: variable binding or enum variant
  case TokenKind::IDENT: {
    bump();
    // If followed by '(', it's a variant: Circle(r)
    if (peek() == TokenKind::OPAREN) {
      bump(); // (
      while (peek() != TokenKind::CPAREN && peek() != TokenKind::_EOF) {
        parse_pattern();
        if (!eat(TokenKind::COMMA))
          break;
      }
      expect(TokenKind::CPAREN);
      m.set_kind(SyntaxKind::ENUM_P);
    } else {
      m.set_kind(SyntaxKind::BIND_P);
    }
    break;
  }

  default:
    error_recover("unexpected token in pattern");
    break;
  }
}

// pattern guard? => block / expr_stmt?
void vd::Parser::parse_match_arm() {
  auto m = open();
  parse_pattern();
  if (peek() == TokenKind::IF) {
    auto gm = open();
    bump(); // if
    parse_expr();
    gm.set_kind(SyntaxKind::GUARD_P);
  }

  expect(TokenKind::FATARROW);
  if (peek() == TokenKind::OBRACE)
    parse_block();
  else {
    parse_expr();
    eat(TokenKind::COMMA);
  }
  m.set_kind(SyntaxKind::MATCH_ARM);
}

// match expr { [match_arms] }
void vd::Parser::parse_match_expr() {
  auto m = open();
  expect(TokenKind::MATCH);
  parse_expr(); // match subject
  if (!expect(TokenKind::OBRACE)) {
    error_recover("expected '{' before match body");
    return;
  }
  while (peek() != TokenKind::CBRACE && peek() != TokenKind::_EOF)
    parse_match_arm();

  if (!expect(TokenKind::CBRACE)) {
    error_recover("expected '}' after match body");
    return;
  }
  m.set_kind(SyntaxKind::MATCH_E);
}

// for in iterable block
void vd::Parser::parse_for_stmt() {
  auto m = open();
  expect(TokenKind::FOR);
  parse_pattern();
  expect(TokenKind::IN);
  parse_expr(0, false);
  parse_block();
  m.set_kind(SyntaxKind::FOR_S);
}

// while condition bloc
void vd::Parser::parse_while_stmt() {
  auto m = open();
  expect(TokenKind::WHILE);
  parse_expr(0, false);
  parse_block();
  m.set_kind(SyntaxKind::WHILE_S);
}

// loop block
void vd::Parser::parse_loop_stmt() {
  auto m = open();
  expect(TokenKind::LOOP);
  parse_block();
  m.set_kind(SyntaxKind::LOOP_S);
}

void vd::Parser::parse_break_stmt() {
  auto m = open();
  expect(TokenKind::BREAK);
  expect(TokenKind::SEMI);
  m.set_kind(SyntaxKind::BREAK);
}

void vd::Parser::parse_continue_stmt() {
  auto m = open();
  expect(TokenKind::CONTINUE);
  expect(TokenKind::SEMI);
  m.set_kind(SyntaxKind::CONTINUE);
}

void vd::Parser::parse_struct_lit() {
  auto m = open();
  eat(TokenKind::IDENT); // Point

  if (peek() == TokenKind::OBRACK)
    parse_gen_args(); // Vec[int]{...}

  if (!expect(TokenKind::OBRACE)) {
    error_recover("expected '{' in struct literal");
    return;
  }

  while (peek() != TokenKind::CBRACE && peek() != TokenKind::_EOF) {
    expect(TokenKind::DOT);   // .field required
    expect(TokenKind::IDENT); // field name
    expect(TokenKind::COL);
    parse_expr(0); // value
    if (!eat(TokenKind::COMMA))
      break;
  }

  if (!expect(TokenKind::CBRACE)) {
    error_recover("expected '}' in struct literal");
    return;
  }
  m.set_kind(SyntaxKind::STRUCTURE_LIT_E);
}

void vd::Parser::amend_last_error(const char *msg) {
  if (!errors_.empty()) {
    errors_.back().message += "; ";
    errors_.back().message += msg;
  }
}

// NOTE: the caller must open and close nodes
void vd::Parser::error_recover(const char *context) {
  amend_last_error(context);
  // auto m = open();
  while (peek() != TokenKind::SEMI && peek() != TokenKind::CBRACE &&
         peek() != TokenKind::COMMA && peek() != TokenKind::_EOF) {
    bump();
  }
  eat(TokenKind::SEMI);
  eat(TokenKind::COMMA);
  // m.set_kind( SyntaxKind::ERROR);
}

void vd::Parser::parse_file() {
  auto m = open();

  while (peek() != TokenKind::_EOF) {
    switch (peek()) {
    case TokenKind::LET:
      parse_inferred_binding();
      break;
    case TokenKind::MUT:
      if (is_typed_binding())
        parse_typed_binding();
      else
        parse_inferred_binding();
      break;
    case TokenKind::TYPE:
      parse_type_def();
      break;
    case TokenKind::STRUCTURE:
      parse_structure();
      break;
    case TokenKind::IMPLEMENT:
      parse_implement();
      break;
    case TokenKind::INTERFACE:
      parse_interface();
      break;
    case TokenKind::IMPORT:
      // TODO: parse_import()
      // error_recover();
      break;
    case TokenKind::IDENT:
      if (is_fn_def())
        parse_fn();
      else
        parse_stmt();
      break;
    default:
      parse_stmt();
      break;
    }
  }

  m.set_kind(SyntaxKind::ROOT);
}

void vd::Parser::raw_dump() const {
  int depth = 0;
  for (size_t i = 0; i < events_.size(); ++i) {
    auto &ev = events_[i];

    if (ev.kind == EventKind::CLOSE)
      depth--;

    std::cout << "[" << std::right << std::setw(3) << std::setfill('0') << i
              << "] " << std::setfill(' ')
              << std::string(static_cast<std::size_t>(depth) * 2, ' ')
              << std::setw(9) << std::left
              << TreeBuilder::event_kind_name(ev.kind) << " "
              << TreeBuilder::syntax_kind_name(ev.syntax_kind) << "\n";

    if (ev.kind == EventKind::OPEN)
      depth++;
  }
}
