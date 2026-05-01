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
    if (is_struct_lit()) {
      parse_struct_lit();
    } else {
      auto m = open();
      bump();
      close(m, SyntaxKind::IDENT_E);
    }
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
    error_recover(); // NOTE: NEED TO TEST THIS
    break;
  }
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
      parse_args();
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

void vd::Parser::parse_type_ref() {
  std::size_t cp = checkpoint();
  auto m = open();

  if (eat(TokenKind::AMP)) {
    // &[type]
    expect(TokenKind::OBRACK);
    parse_type_ref();
    expect(TokenKind::CBRACK);
    close(m, SyntaxKind::SLICE_TYPE_REF);
  } else if (peek() == TokenKind::OBRACK) {
    // [type] or [type; size]
    bump();
    parse_type_ref();
    if (eat(TokenKind::SEMI))
      parse_expr();
    expect(TokenKind::CBRACK);
    close(m, SyntaxKind::ARRAY_TYPE_REF);
  } else if (peek() == TokenKind::OPAREN && peek(1) == TokenKind::CPAREN) {
    // () - the unit return type
    bump(); // (
    bump(); // )
    close(m, SyntaxKind::VOID);
  } else {
    // any named type: int, float, Point, T, ...
    expect(TokenKind::IDENT);
    if (peek() == TokenKind::OBRACK)
      parse_gen_args(); // Vec[int]
    close(m, SyntaxKind::TYPE_REF);
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
  parse_type_ref();
  expect(TokenKind::IDENT); // var name
  expect(TokenKind::ASSIGN);
  parse_expr();
  expect(TokenKind::SEMI);
  close(m, SyntaxKind::TYPED_BINDING);
}

void vd::Parser::parse_inferred_binding() {
  auto m = open();
  bump();                   // let or mut
  expect(TokenKind::IDENT); // var name
  expect(TokenKind::ASSIGN);
  parse_expr();
  expect(TokenKind::SEMI);
  close(m, SyntaxKind::INFERRED_BINDING);
}

// NOTE: ----------------------------------

void vd::Parser::parse_return_stmt() {
  auto m = open();
  expect(TokenKind::RETURN);
  if (peek() != TokenKind::SEMI)
    parse_expr();
  expect(TokenKind::SEMI);
  close(m, SyntaxKind::RETURN_S);
}

void vd::Parser::parse_block() {
  auto m = open();
  expect(TokenKind::OBRACE);

  while (peek() != TokenKind::CBRACE && peek() != TokenKind::_EOF) {
    bool terminated = parse_stmt();
    if (!terminated) {
      // parse_return_stmt();
      break;
    }
  }

  expect(TokenKind::CBRACE);
  close(m, SyntaxKind::BLOCK_E);
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
      close(m, SyntaxKind::EXPR_S);
      return true;
    } else {
      close(m, SyntaxKind::IMPLICIT_RETURN_E);
      return false;
    }
  }
  case TokenKind::MATCH: {
    auto m = open();
    parse_match_expr();
    if (eat(TokenKind::SEMI)) {
      close(m, SyntaxKind::EXPR_S);
      return true;
    } else {
      close(m, SyntaxKind::IMPLICIT_RETURN_E);
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
      close(m, SyntaxKind::EXPR_S);
      return true;
    } else {
      close(m, SyntaxKind::IMPLICIT_RETURN_E);
      return false;
    }
  }
  case TokenKind::STRUCT:
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
  case TokenKind::INT:
  case TokenKind::FLOAT:
  case TokenKind::STRING:
  case TokenKind::CHAR:
  case TokenKind::TRUE:
  case TokenKind::FALSE:
  case TokenKind::_NULL:
  case TokenKind::OPAREN:
  case TokenKind::OBRACK:
  case TokenKind::AMP:
  case TokenKind::STAR:
  case TokenKind::MINUS:
    // case TokenKind::PIPE: { // lambda |x| -> ...
  case TokenKind::BANG: {
    auto m = open();
    parse_expr(0);
    if (eat(TokenKind::SEMI)) {
      close(m, SyntaxKind::EXPR_S);
      return true;
    } else {
      close(m, SyntaxKind::IMPLICIT_RETURN_E);
      return false; // implicit return
    }
  }

  default:
    error_recover();
    return true;
  }
}

void vd::Parser::parse_params() {
  auto m = open();
  expect(TokenKind::OPAREN);
  while (peek() != TokenKind::CPAREN && peek() != TokenKind::_EOF) {
    if (peek() == TokenKind::SELF) {
      auto pm = open();
      bump();
      close(pm, SyntaxKind::SELF_PARAM);
    } else {
      auto pm = open();
      parse_type_ref();
      expect(TokenKind::IDENT);
      close(pm, SyntaxKind::PARAM);
    }
    if (!eat(TokenKind::COMMA))
      break;
  }
  expect(TokenKind::CPAREN);
  close(m, SyntaxKind::PARAM_LIST);
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
  close(m, SyntaxKind::ARG_LIST);
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
  close(m, SyntaxKind::GEN_ARG_LIST);
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
  expect(TokenKind::CBRACK);
  close(m, SyntaxKind::GEN_PARAM_LIST);
}

void vd::Parser::parse_fn() {
  auto m = open();
  expect(TokenKind::IDENT);
  if (peek() == TokenKind::OBRACK)
    parse_gen_params();
  expect(TokenKind::OPAREN);
  parse_params();
  if (eat(TokenKind::ARROW)) // optional arrow if () return
    parse_type_ref();        // return type
  if (eat(TokenKind::SEMI))
    close(m, SyntaxKind::FN_SIG);
  else { // start of fn def
    parse_block();
    close(m, SyntaxKind::FN_D);
  }
}

void vd::Parser::parse_field() {
  auto m = open();
  [[maybe_unused]] bool is_export = eat(TokenKind::EXPORT);
  expect(TokenKind::IDENT); // field name
  expect(TokenKind::COL);
  parse_type_ref();
  eat(TokenKind::COMMA);
  close(m, SyntaxKind::FIELD_E);
}

void vd::Parser::parse_structure() {
  auto m = open();
  expect(TokenKind::STRUCT);
  expect(TokenKind::IDENT); // name
  if (peek() == TokenKind::OBRACK)
    parse_gen_params(); // [T, U]
  expect(TokenKind::OBRACE);

  while (peek() != TokenKind::CBRACE && peek() != TokenKind::_EOF) {
    if (is_fn_def())
      parse_fn(); // method sig → ends with ; → FN_SIG
    else
      parse_field(); // field → name: type,
  }

  expect(TokenKind::CBRACE);
  close(m, SyntaxKind::STRUCTURE_D);
}

void vd::Parser::parse_implement() {
  auto m = open();
  expect(TokenKind::IMPLEMENT);
  expect(TokenKind::IDENT);
  expect(TokenKind::OBRACE);
  while (peek() != TokenKind::CBRACE && peek() != TokenKind::_EOF)
    parse_fn(); // always full def here, never sig
  expect(TokenKind::CBRACE);
  close(m, SyntaxKind::IMPLEMENT_D);
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
  close(m, SyntaxKind::VARIANT_D);
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
  close(m, SyntaxKind::TYPE_D);
}

void vd::Parser::parse_interface() {
  auto m = open();
  expect(TokenKind::INTERFACE);
  expect(TokenKind::IDENT);
  if (peek() == TokenKind::OBRACK)
    parse_gen_params(); // handles the operator overload or generic case

  expect(TokenKind::OBRACE);
  while (peek() != TokenKind::CBRACE && peek() != TokenKind::_EOF) {
    if (is_fn_def())
      parse_fn(); // method sig, must end with ;
    else {
      // interface inclusion: Hashable;
      expect(TokenKind::IDENT);
      expect(TokenKind::SEMI);
    }
  }
  expect(TokenKind::CBRACE);
  close(m, SyntaxKind::INTERFACE_D);
}

void vd::Parser::parse_if_expr() {
  auto m = open();
  expect(TokenKind::IF);
  parse_expr();
  parse_block();
  if (eat(TokenKind::ELSE)) {
    if (peek() == TokenKind::IF)
      parse_if_expr();
    else
      parse_block();
  }
  close(m, SyntaxKind::IF_E);
}

// WILDCARD_P, BIND_P, TUPLE_P, ENUM_P, GUARD_P, OR_P,
void vd::Parser::parse_pattern() {
  auto m = open();

  switch (peek()) {
  // wildcard
  case TokenKind::UND:
    bump();
    close(m, SyntaxKind::WILDCARD_P);
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
    close(m, SyntaxKind::LITERAL_P);
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
    close(m, SyntaxKind::TUPLE_P);
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
      close(m, SyntaxKind::ENUM_P);
    } else {
      close(m, SyntaxKind::BIND_P);
    }
    break;
  }

  default:
    error_recover();
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
    close(gm, SyntaxKind::GUARD_P);
  }

  expect(TokenKind::FATARROW);
  if (peek() == TokenKind::OBRACE)
    parse_block();
  else {
    parse_expr();
    eat(TokenKind::COMMA);
  }
  close(m, SyntaxKind::MATCH_ARM);
}

// match expr { [match_arms] }
void vd::Parser::parse_match_expr() {
  auto m = open();
  expect(TokenKind::MATCH);
  parse_expr(); // match subject
  expect(TokenKind::OBRACE);
  while (peek() != TokenKind::CBRACE && peek() != TokenKind::_EOF)
    parse_match_arm();

  expect(TokenKind::CBRACE);
  close(m, SyntaxKind::MATCH_E);
}

// for in iterable block
void vd::Parser::parse_for_stmt() {
  auto m = open();
  expect(TokenKind::FOR);
  parse_pattern();
  expect(TokenKind::IN);
  parse_expr();
  parse_block();
  close(m, SyntaxKind::FOR_S);
}

// while condition bloc
void vd::Parser::parse_while_stmt() {
  auto m = open();
  expect(TokenKind::WHILE);
  parse_expr();
  parse_block();
  close(m, SyntaxKind::WHILE_S);
}

// loop block
void vd::Parser::parse_loop_stmt() {
  auto m = open();
  expect(TokenKind::LOOP);
  parse_block();
  close(m, SyntaxKind::LOOP_S);
}

void vd::Parser::parse_break_stmt() {
  auto m = open();
  expect(TokenKind::BREAK);
  expect(TokenKind::SEMI);
  close(m, SyntaxKind::BREAK);
}

void vd::Parser::parse_continue_stmt() {
  auto m = open();
  expect(TokenKind::CONTINUE);
  expect(TokenKind::SEMI);
  close(m, SyntaxKind::CONTINUE);
}

void vd::Parser::parse_struct_lit() {
  auto m = open();
  expect(TokenKind::IDENT); // Point

  if (peek() == TokenKind::OBRACK)
    parse_gen_args(); // Vec[int]{...}

  expect(TokenKind::OBRACE);

  while (peek() != TokenKind::CBRACE && peek() != TokenKind::_EOF) {
    expect(TokenKind::DOT);   // .field required
    expect(TokenKind::IDENT); // field name
    expect(TokenKind::COL);
    parse_expr(0); // value
    if (!eat(TokenKind::COMMA))
      break;
  }

  expect(TokenKind::CBRACE);
  close(m, SyntaxKind::STRUCT_LIT_E);
}

void vd::Parser::error_recover() {
  auto m = open();
  while (peek() != TokenKind::SEMI && peek() != TokenKind::CBRACE &&
         peek() != TokenKind::_EOF) {
    bump();
  }
  eat(TokenKind::SEMI);
  close(m, SyntaxKind::ERROR);
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
    case TokenKind::STRUCT:
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
      error_recover();
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

  close(m, SyntaxKind::ROOT);
}

void vd::Parser::raw_dump() const {
  for (size_t i = 0; i < events_.size(); ++i) {
    std::cout << "[" << std::right << std::setw(3) << std::setfill('0') << i
              << "] " << std::setfill(' ') << std::setw(9) << std::left
              << TreeBuilder::event_kind_name(events_[i].kind) << " "
              << TreeBuilder::syntax_kind_name(events_[i].syntax_kind) << "\n";
  }
}
