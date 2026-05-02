#pragma once

#include "token.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <memory_resource>
#include <span>
#include <sys/types.h>
#include <variant>
#include <vector>

namespace vd {
enum class EventKind {
  OPEN,      // start node
  CLOSE,     // end node
  TOKEN,     // consume next token
  TOMBSTONE, // builder skip node
};

// clang-format off
enum class SyntaxKind : uint32_t {
#define AS_SYNTAX(kind) kind,
    TOKEN_T(AS_SYNTAX)
#undef AS_SYNTAX
    // expressions
    BINARY_E, UNARY_E, CALL_E, LITERAL_E, IDENT_E, PAREN_E,
    FIELD_E, INDEX_E, PIPELINE_E, IF_E, MATCH_E,
    BLOCK_E, LAMBDA_E, RANGE_E, STRUCTURE_LIT_E,
    TUPLE_E, IMPLICIT_RETURN_E,
    // statements
    LET_S, MUT_S, RETURN_S, EXPR_S, TYPED_BINDING,
    INFERRED_BINDING, FOR_S, WHILE_S, LOOP_S,
    // declarations
    FN_D, STRUCTURE_D, INTERFACE_D, TYPE_D,
    IMPLEMENT_D, EFFECT_D,
    FIELD_D, VARIANT_D, FN_SIG,
    // patterns
    WILDCARD_P, BIND_P, TUPLE_P, ENUM_P, GUARD_P, OR_P, LITERAL_P,
    // misc
    SELF_PARAM, PARAM, PARAM_LIST, ARG_LIST, TYPE_REF, GEN_PARAM_LIST,
    OPTIONAL_TYPE_REF, SLICE_TYPE_REF, ARRAY_TYPE_REF,
    UNION_TYPE_REF, GEN_ARG_LIST, MATCH_ARM,
    ROOT, ARRAY_LIT
};
// clang-format on

struct Event {
  EventKind kind;
  SyntaxKind syntax_kind; // for OPEN
};

// forward declaration
struct GreenNode;

// a child is either a node or a token
using GreenElement = std::variant<GreenNode *, const Token *>;

struct GreenNode {
  SyntaxKind kind;
  uint32_t text_len;
  std::span<GreenElement> children; // allocated in arena
};

class TreeBuilder {
public:
  TreeBuilder(std::span<Event> events, std::span<const Token> tokens)
      : events_(events), tokens_(tokens), stack_(&pool_) {}

  GreenNode *build() {
    GreenNode *root_node = nullptr;
    for (size_t i = 0; i < events_.size(); ++i) {
      const auto &event = events_[i];
      switch (event.kind) {

      case EventKind::OPEN:
        stack_.emplace_back(Frame(event.syntax_kind, &pool_));
        break;

      case EventKind::TOKEN: {
        const auto &token = tokens_[cursor_++];
        stack_.back().children.emplace_back(&token);
        stack_.back().text_len += static_cast<uint32_t>(token.text.size());
      } break;

      case EventKind::CLOSE: {
        Frame top = std::move(stack_.back());
        stack_.pop_back();

        GreenElement *children_storage = nullptr;
        if (!top.children.empty()) {
          children_storage = static_cast<GreenElement *>(
              pool_.allocate(sizeof(GreenElement) * top.children.size(),
                             alignof(GreenElement)));
          std::uninitialized_copy(top.children.begin(), top.children.end(),
                                  children_storage);
        }
        GreenNode *node = static_cast<GreenNode *>(
            pool_.allocate(sizeof(GreenNode), alignof(GreenNode)));
        new (node) GreenNode{
            top.kind,
            top.text_len,
            std::span<GreenElement>(children_storage, top.children.size()),
        };

        if (stack_.empty())
          root_node = node;
        else {
          stack_.back().children.emplace_back(node);
          stack_.back().text_len += node->text_len;
        }
      } break;
      default:
        break;
      }
    }
    // debug info (check stack balance)
    assert(stack_.empty() && "unbalanced OPEN/CLOSE in event stream");
    assert(root_node != nullptr && "build() produced no root");
    return root_node;
  }

  static inline const char *event_kind_name(EventKind k) {
    switch (k) {
    case EventKind::OPEN:
      return "OPEN";
    case EventKind::CLOSE:
      return "CLOSE";
    case EventKind::TOKEN:
      return "TOKEN";
    case EventKind::TOMBSTONE:
      return "TOMBSTONE";
    default:
      return "UNKNOWN";
    }
  }

  static inline const char *syntax_kind_name(SyntaxKind k) {
    switch (k) {
      // clang-format off
#define AS_CASE(kind) case SyntaxKind::kind: return #kind;
    TOKEN_T(AS_CASE)
#undef AS_CASE
      // clang-format on

    // Expressions
    case SyntaxKind::BINARY_E:
      return "BINARY_E";
    case SyntaxKind::UNARY_E:
      return "UNARY_E";
    case SyntaxKind::CALL_E:
      return "CALL_E";
    case SyntaxKind::LITERAL_E:
      return "LITERAL_E";
    case SyntaxKind::IDENT_E:
      return "IDENT_E";
    case SyntaxKind::PAREN_E:
      return "PAREN_E";
    case SyntaxKind::FIELD_E:
      return "FIELD_E";
    case SyntaxKind::INDEX_E:
      return "INDEX_E";
    case SyntaxKind::PIPELINE_E:
      return "PIPELINE_E";
    case SyntaxKind::IF_E:
      return "IF_E";
    case SyntaxKind::MATCH_E:
      return "MATCH_E";
    case SyntaxKind::BLOCK_E:
      return "BLOCK_E";
    case SyntaxKind::LAMBDA_E:
      return "LAMBDA_E";
    case SyntaxKind::RANGE_E:
      return "RANGE_E";
    case SyntaxKind::STRUCTURE_LIT_E:
      return "STRUCTURE_LIT_E";
    case SyntaxKind::TUPLE_E:
      return "TUPLE_E";
    case SyntaxKind::IMPLICIT_RETURN_E:
      return "IMPLICIT_RETURN_E";

    // Statements
    case SyntaxKind::LET_S:
      return "LET_S";
    case SyntaxKind::MUT_S:
      return "MUT_S";
    case SyntaxKind::RETURN_S:
      return "RETURN_S";
    case SyntaxKind::EXPR_S:
      return "EXPR_S";
    case SyntaxKind::TYPED_BINDING:
      return "TYPED_BINDING";
    case SyntaxKind::INFERRED_BINDING:
      return "INFERRED_BINDING";
    case SyntaxKind::FOR_S:
      return "FOR_S";
    case SyntaxKind::WHILE_S:
      return "WHILE_S";
    case SyntaxKind::LOOP_S:
      return "LOOP_S";

    // Declarations
    case SyntaxKind::FN_D:
      return "FN_D";
    case SyntaxKind::STRUCTURE_D:
      return "STRUCTURE_D";
    case SyntaxKind::INTERFACE_D:
      return "INTERFACE_D";
    case SyntaxKind::TYPE_D:
      return "TYPE_D";
    case SyntaxKind::IMPLEMENT_D:
      return "IMPLEMENT_D";
    case SyntaxKind::EFFECT_D:
      return "EFFECT_D";
    case SyntaxKind::FIELD_D:
      return "FIELD_D";
    case SyntaxKind::VARIANT_D:
      return "VARIANT_D";
    case SyntaxKind::FN_SIG:
      return "FN_SIG";

    // Patterns
    case SyntaxKind::WILDCARD_P:
      return "WILDCARD_P";
    case SyntaxKind::BIND_P:
      return "BIND_P";
    case SyntaxKind::TUPLE_P:
      return "TUPLE_P";
    case SyntaxKind::ENUM_P:
      return "ENUM_P";
    case SyntaxKind::GUARD_P:
      return "GUARD_P";
    case SyntaxKind::OR_P:
      return "OR_P";
    case SyntaxKind::LITERAL_P:
      return "LITERAL_P";

    // Misc
    case SyntaxKind::SELF_PARAM:
      return "SELF_PARAM";
    case SyntaxKind::PARAM:
      return "PARAM";
    case SyntaxKind::PARAM_LIST:
      return "PARAM_LIST";
    case SyntaxKind::ARG_LIST:
      return "ARG_LIST";
    case SyntaxKind::TYPE_REF:
      return "TYPE_REF";
    case SyntaxKind::GEN_PARAM_LIST:
      return "GEN_PARAM_LIST";
    case SyntaxKind::OPTIONAL_TYPE_REF:
      return "OPTIONAL_TYPE_REF";
    case SyntaxKind::SLICE_TYPE_REF:
      return "SLICE_TYPE_REF";
    case SyntaxKind::ARRAY_TYPE_REF:
      return "ARRAY_TYPE_REF";
    case SyntaxKind::UNION_TYPE_REF:
      return "UNION_TYPE_REF";
    case SyntaxKind::GEN_ARG_LIST:
      return "GEN_ARG_LIST";
    case SyntaxKind::MATCH_ARM:
      return "MATCH_ARM";
    case SyntaxKind::ARRAY_LIT:
      return "ARRAY_LIT_E";
    case SyntaxKind::ROOT:
      return "ROOT";

    default:
      return "UNKNOWN";
    }
  }

private:
  std::span<Event> events_;
  std::span<const Token> tokens_;
  std::size_t cursor_ = 0;

  // TODO: take memory resource from main
  std::pmr::monotonic_buffer_resource pool_;

  struct Frame {
    SyntaxKind kind;
    std::pmr::vector<GreenElement> children;
    uint32_t text_len = 0;

    Frame(SyntaxKind k, std::pmr::memory_resource *r) : kind(k), children(r) {}
  };

  std::pmr::vector<Frame> stack_;
};
} // namespace vd
