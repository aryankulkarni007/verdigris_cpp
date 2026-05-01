# parser

**Phase 1 — parser primitives (do first):**

- (X) is_fn_def()
- (X) is_typed_binding()
- ( ) parse_expr_stmt()
- (X) parse_typed_binding()
- (X) parse_return_stmt()
- (X) parse_block()
- (X) parse_stmt() -- dispatcher, needs all above

**Phase 2 — declarations:**

- (X) parse_args()
- (X) parse_gen_args()
- (X) parse_fn()
- (X) parse_field()
- (X) parse_structure()
- (X) parse_implement()
- (X) parse_type_decl() -- ADT: type Shape = ...
- (X) parse_interface()

**Phase 3 — expressions (missing pieces):**

- (X) parse_args()
- (X) parse_gen_args()
- (X) parse_if_expr()
- (X) parse_match_expr() -- arms, guards
- (X) parse_for_expr()
- (X) parse_while_expr()
- (X) parse_loop_expr()
- (X) parse_struct_lit() -- Point { .x: 0, .y: 0 }
- (~) parse_lambda() -- deferred, syntax not decided

**Phase 4 — patterns:**

- (X) parse\*pattern()

**Phase 5 — error recovery:**

- ( ) error_recover() -- skip to sync point
- ( ) expect() upgrade -- emit ERROR node not just push error
- ( ) parse_stmt() guard -- wrap bad stmts in ERROR node

**Phase 6 — wire up:**

parse_file() -- ROOT loop, calls parse_stmt until EOF
Lexer::lex_all() -- already designed, implement if not done
Parser::finish() -- hand events to TreeBuilder, return GreenNode\*
main() -- lex → parse → build → print tree

**Phase 7 — tree printer:**

print_tree(GreenNode\*, tokens, indent) -- recursive debug printer

**After all that — design talk:**

error messages -- "expected X found Y at line:col"
effects system
lambda syntax
module system
memory model / pools
comptime eval
macros
