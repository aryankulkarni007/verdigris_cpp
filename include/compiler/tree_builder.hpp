#pragma once

#include "compiler/token.hpp"
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
  // tokens
#define AS_SYNTAX(kind) kind,
    TOKEN_T(AS_SYNTAX)
#undef AS_SYNTAX

  // expressions
  BINARY_E, UNARY_E, CALL_E, LITERAL_E, IDENT_E, PAREN_E, FIELD_E, INDEX_E,
  PIPELINE_E, MATCH_EXPR, IF_EXPR,
  BLOCK_EXPR,
  // statments
  LET_STMT, MUT_STMT, RETURN_STMT, EXPR_STMT,
  // declarations
  FN_DEF, STRUCT_DEF, ENUM_DEF, INTERFACE_DEF,
  // patterns
  WILDCARD_PAT, BIND_PAT, TUPLE_PAT, ENUM_PAT,
  // misc
  PARAM_LIST, ARG_LIST, TYPE_REF,
  ROOT,       // top of tree
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
    case SyntaxKind::BINARY_E:
      return "BINARY_E";
    case SyntaxKind::LITERAL_E:
      return "LITERAL_E";
    case SyntaxKind::UNARY_E:
      return "UNARY_E";
    case SyntaxKind::CALL_E:
      return "CALL_E";
    case SyntaxKind::ROOT:
      return "ROOT";
    // etc
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
