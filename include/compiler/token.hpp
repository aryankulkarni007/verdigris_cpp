#pragma once

#include <cstdint>
#include <iomanip>
#include <string_view>
#define TOKEN_T(X)                                                             \
  /* ── special ────────────────────────────────────────────────────────── */  \
  X(ILLEGAL) /* unknown / unrecognised character                     */        \
  X(_EOF)    /* end of file                                          */        \
  /* ── literals ───────────────────────────────────────────────────────── */  \
  X(IDENT)  /* identifier or keyword-as-ident: foo, int, let, ...   */         \
  X(INT)    /* integer literal: 42, 0, 1000                         */         \
  X(FLOAT)  /* float literal: 3.14, 0.5                             */         \
  X(STRING) /* string literal: "hello"                              */         \
  X(CHAR)   /* char literal: 'a'                                    */         \
  X(TRUE)   /* boolean true  (lexed as ident, reserved for later)   */         \
  X(FALSE)  /* boolean false (lexed as ident, reserved for later)   */         \
  /* ── keywords ───────────────────────────────────────────────────────── */  \
  X(AS)        /* as    — type cast: x as float                        */      \
  X(LET)       /* let   — immutable inferred binding: let x = 1        */      \
  X(MUT)       /* mut   — mutable binding: mut x = 1                   */      \
  X(STRUCT)    /* structure — struct declaration                          */   \
  X(TYPE)      /* type  — 'enum' but actually union type declaration   */      \
  X(FOR)       /* for   — for loop                                     */      \
  X(IN)        /* in    — for i in array                               */      \
  X(MATCH)     /* match — pattern match expression                     */      \
  X(LOOP)      /* loop  — infinite loop                                */      \
  X(WHILE)     /* while — condition loop                               */      \
  X(BREAK)     /* break — exit loop                                    */      \
  X(CONTINUE)  /* continue — skip to next iteration                    */      \
  X(IF)        /* if    — conditional / if expression                  */      \
  X(ELSE)      /* else  — else branch                                  */      \
  X(RETURN)    /* return — explicit early return                       */      \
  X(_NULL)     /* null  — null type / null value                       */      \
  X(VOID)      /* void  — unit return type                             */      \
  X(SELF)      /* self  — receiver in method definitions               */      \
  X(EXPORT)    /* export   — public visibility modifier                */      \
  X(IMPORT)    /* import — module import                               */      \
  X(INTERFACE) /* interface — structural contract definition           */      \
  X(IMPLEMENT) /* implement — method implementation block              */      \
  X(EFFECT)    /* effect — effect declaration                          */      \
  /* ── assignment ─────────────────────────────────────────────────────── */  \
  X(ASSIGN) /* =    — assignment / binding                          */         \
  /* ── comparison ─────────────────────────────────────────────────────── */  \
  X(EQ)   /* ==   — equality                                        */         \
  X(NEQ)  /* !=   — inequality                                      */         \
  X(LT)   /* <    — less than                                       */         \
  X(GT)   /* >    — greater than                                    */         \
  X(LTEQ) /* <=   — less than or equal                              */         \
  X(GTEQ) /* >=   — greater than or equal                           */         \
  /* ── logical ────────────────────────────────────────────────────────── */  \
  X(AND)  /* &&   — logical and                                     */         \
  X(OR)   /* ||   — logical or                                      */         \
  X(BANG) /* !    — logical not / error propagation suffix          */         \
  /* ── arithmetic ─────────────────────────────────────────────────────── */  \
  X(PLUS)   /* +    — addition                                      */         \
  X(MINUS)  /* -    — subtraction / negation                        */         \
  X(STAR)   /* *    — multiplication / pointer dereference          */         \
  X(SLASH)  /* /    — division                                      */         \
  X(MODULO) /* %    — modulo                                        */         \
  X(SHL)    /* <<   — bitwise shift left                            */         \
  X(SHR)    /* >>   — bitwise shift right                           */         \
  X(CARET)  /* ^    — bitwise shift right                           */         \
  /* ── compound assignment ────────────────────────────────────────────── */  \
  X(PLUSEQ)   /* +=   — add and assign                              */         \
  X(MINUSEQ)  /* -=   — subtract and assign                         */         \
  X(STAREQ)   /* *=   — multiply and assign                         */         \
  X(SLASHEQ)  /* /=   — divide and assign                           */         \
  X(MODULOEQ) /* %=   — modulo and assign                           */         \
  X(CARETEQ)  /* ^    — bitwise shift right                           */       \
  /* ── delimiters ─────────────────────────────────────────────────────── */  \
  X(OPAREN) /* (    — open parenthesis                              */         \
  X(CPAREN) /* )    — close parenthesis                             */         \
  X(OBRACE) /* {    — open brace                                    */         \
  X(CBRACE) /* }    — close brace                                   */         \
  X(OBRACK) /* [    — open bracket                                  */         \
  X(CBRACK) /* ]    — close bracket                                 */         \
  /* ── punctuation ────────────────────────────────────────────────────── */  \
  X(SEMI)     /* ;    — statement terminator                          */       \
  X(COMMA)    /* ,    — separator                                     */       \
  X(DOT)      /* .    — field access / method call                    */       \
  X(DDOT)     /* ..   — range operator: 0..10                         */       \
  X(DDOTEQ)   /* ..=  — inclusive range: 0..=10                      */        \
  X(COL)      /* :    — struct field separator in literals: { x: 0 }  */       \
  X(CCOL)     /* ::   — namespace / method path: Point::distance      */       \
  X(AMP)      /* &    — reference / slice prefix: &v, &[int]          */       \
  X(PIPE)     /* |    — union return type: int | error and lambda     */       \
  X(PIPELINE) /* |>   — left-to-right pipeline composition            */       \
  X(ARROW)    /* ->   — function return type: add(a, b) -> int        */       \
  X(FATARROW) /* =>   — match arm: 0 => 1                             */       \
  X(UND)      /* _    — wildcard / discard pattern in match           */       \
  X(HASH)     /* #    - unknown usage                                 */       \
  X(TILDE)    /* ~    - unknown usage                                 */       \
  /* ── attributes ─────────────────────────────────────────────────────── */  \
  X(AT) /* @    — attribute prefix: @inline, @extern            */             \
  /* ── optional / error handling ──────────────────────────────────────── */  \
  X(QUESTION) /* ?    — optional type suffix: ?int                    */       \
  X(ERROR)    /* for error handling */

namespace vd {
enum class TokenKind : uint32_t {
#define AS_ENUM(kind) kind,
  TOKEN_T(AS_ENUM)
#undef AS_ENUM
};

inline const char *token_name(TokenKind kind) {
  // clang-format off
  switch (kind) {
#define AS_CASE(kind) case TokenKind::kind: return #kind;
        TOKEN_T(AS_CASE)
#undef AS_CASE
        default: return "UNKNOWN";
    }
  // clang-format on
}

struct SourceLocation {
  uint32_t line;
  uint32_t col;
  uint32_t offset;
};

struct Token {
  TokenKind kind;
  SourceLocation loc;
  std::string_view text;
  std::string_view trivia; // leading
};

inline std::ostream &operator<<(std::ostream &os, TokenKind kind) {
  // clang-format off
  switch (kind) {
#define X(name) case TokenKind::name: return os << #name;
    TOKEN_T(X)
#undef X
    default: return os << "UNKNOWN_TOKEN";
  }
  // clang-format on
}

inline std::ostream &operator<<(std::ostream &os, const Token &token) {
  std::ios_base::fmtflags f(os.flags());
  os << "[" << std::right << std::setw(3) << std::setfill('0') << token.loc.line
     << ":" << std::setw(3) << std::setfill('0') << token.loc.col << "]  ";
  os << std::left << std::setfill(' ') << std::setw(12)
     << token_name(token.kind);
  std::string quoted_text = "'" + std::string(token.text) + "'";
  os << std::setw(15) << quoted_text;

  os.flags(f);
  return os;
}
} // namespace vd
