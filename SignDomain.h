//===- SignDomain.h - The abstract domain ---------------------------------===//
//
// A cartesian lattice recording sign of an integer value.
//
// This is the file to replace first when building a different analysis.  MLIR's
// dataflow framework asks only three things of a lattice value:
//
//   * a default constructor, which must produce the bottom element, because the
//     solver starts every value optimistically and lowers it as facts arrive;
//   * a static join(), which must be commutative, associative, idempotent, and
//     monotone -- assertions in Lattice<> check monotonicity in debug builds;
//   * operator== and print().
//
//===----------------------------------------------------------------------===//

#ifndef SIGN_DOMAIN_H
#define SIGN_DOMAIN_H

#include "llvm/Support/raw_ostream.h"
#include <unordered_set>

namespace sign {

enum class Sign { Zero, Positive, Negative };

inline std::string name(Sign kind) {
  switch (kind) {
  case Sign::Zero:
    return "zero";
  case Sign::Positive:
    return "positive";
  case Sign::Negative:
    return "negative";
  default:
    return "unknown";
  }
}

struct SignState {
  std::unordered_set<Sign> signs;

  SignState() = default;
  SignState(std::initializer_list<Sign> signs) {
    for (auto sign : signs) {
      this->signs.insert(sign);
    }
  }
  SignState(Sign sign) : SignState({sign}) { }
  static SignState bottom() { return SignState(); }
  static SignState top() {
    return SignState({Sign::Zero, Sign::Positive, Sign::Negative});
  }
  bool isBottom() const { return signs.empty(); }
  bool isTop() const { return (*this) == top(); }
  bool hasSign(Sign sign) const { return signs.count(sign) > 0; }

  static SignState join(const SignState &lhs, const SignState &rhs) {
    SignState result;
    for (auto sign : lhs.signs) {
        result.signs.insert(sign);
    }
    for (auto sign : rhs.signs) {
        result.signs.insert(sign);
    }
    return result;
  }

  static SignState meet(const SignState &lhs, const SignState &rhs) {
    SignState result;
    for (auto sign : lhs.signs) {
      if (rhs.hasSign(sign)) {
        result.signs.insert(sign);
      }
    }
    return result;
  }

  static SignState transfer(const SignState &lhs, const SignState &rhs, const std::function<SignState(const Sign &, const Sign &)> &func) {
    SignState result;
    for (auto x : lhs.signs) {
      for (auto y : rhs.signs) {
        result = join(result, func(x, y));
      }
    }
    return result;
  }

  bool operator==(const SignState &other) const { return signs == other.signs; }
  bool operator!=(const SignState &other) const { return signs != other.signs; }

  void print(llvm::raw_ostream &os) const { 
    os << "{ "; 
    for (auto sign : signs) { 
      os << name(sign); 
      os << ", "; 
    } 
    os << "}"; 
  } 
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                                     const SignState &state) {
  state.print(os);
  return os;
}

} // namespace sign

#endif
