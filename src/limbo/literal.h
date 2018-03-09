// vim:filetype=cpp:textwidth=120:shiftwidth=2:softtabstop=2:expandtab
// Copyright 2014-2017 Christoph Schwering
// Licensed under the MIT license. See LICENSE file in the project root.
//
// A literal is an (in)equality expression of two terms. Literals are immutable.
// If one of either terms in a literal is a function, then the left-hand side
// is a function.
//
// The most important operations are Complementary() and [Properly]Subsumes()
// checks, which are only defined for primitive literals. Note that the
// operations PropagateUnit() and Subsumes() from the Clause class use hashing to
// speed them up and therefore depend on their inner workings. In other words:
// when you modify them, double-check with the Clause class.
//
// Due to the memory-wise lightweight representation of terms, copying or
// comparing literals is very fast.

#ifndef LIMBO_LITERAL_H_
#define LIMBO_LITERAL_H_

#include <cassert>

#include <functional>
#include <utility>

#include <limbo/term.h>

#include <limbo/internal/ints.h>
#include <limbo/internal/maybe.h>

namespace limbo {

class Literal {
 public:
  template<typename T>
  using Maybe = internal::Maybe<T>;
  struct LhsHash;

  static Literal Eq(Term lhs, Term rhs) { return Literal(true, lhs, rhs); }
  static Literal Neq(Term lhs, Term rhs) { return Literal(false, lhs, rhs); }

  Literal() = default;

  Term lhs() const { return Term(static_cast<u32>(data_)); }
  bool pos() const { return (data_ >> 63) == 1; }
  Term rhs() const { return Term(static_cast<u32>(data_ >> 32) & ~static_cast<u32>(1 << 31)); }

  bool null()            const { return data_ == 0; }
  bool ground()          const { return lhs().ground() && rhs().ground(); }
  bool trivial()         const { return lhs().name() && rhs().name(); }
  bool primitive()       const { return lhs().primitive() && rhs().name(); }
  bool quasi_trivial()   const { return lhs().quasi_name() && rhs().quasi_name(); }
  bool quasi_primitive() const { return lhs().quasi_primitive() && rhs().quasi_name(); }
  bool well_formed()     const { return quasi_trivial() || quasi_primitive(); }

  Literal flip() const { return Literal(!pos(), lhs(), rhs()); }
  Literal dual() const { return Literal(pos(), rhs(), lhs()); }

  bool operator==(Literal a) const { return pos() == a.pos() && lhs() == a.lhs() && rhs() == a.rhs(); }
  bool operator!=(Literal a) const { return !(*this == a); }
  bool operator<(Literal a) const { return lhs() < a.lhs() || (lhs() == a.lhs() && data_ < a.data_); }

  static Literal Min(Term lhs) { return Literal(lhs); }

  internal::hash32_t hash() const {
    return internal::jenkins_hash(static_cast<u32>(data_ >> 32)) ^
           internal::jenkins_hash(static_cast<u32>(data_));
  }

  // valid() holds for (t = t) and (n1 != n2) and (t1 != t2) if t1, t2 have different sorts.
  bool valid() const {
    return (pos() && lhs() == rhs()) ||
           (!pos() && lhs().name() && rhs().name() && lhs() != rhs()) ||
           (!pos() && lhs().sort() != rhs().sort());
  }

  // unsatisfiable() holds for (t != t) and (n1 = n2) and (t1 = t2) if t1, t2 have different sorts.
  bool unsatisfiable() const {
    return (!pos() && lhs() == rhs()) ||
           (pos() && lhs().name() && rhs().name() && lhs() != rhs()) ||
           (pos() && lhs().sort() != rhs().sort());
  }

  // Valid(a, b) holds when a, b match one of the following:
  // (t1 = t2), (t1 != t2)
  // (t1 != t2), (t1 = t2)
  // (t1 != n1), (t1 != n2) for distinct n1, n2.
  static bool Valid(const Literal a, const Literal b) {
    assert(a.primitive());
    assert(b.primitive());
    return (a.lhs() == b.lhs() && a.pos() != b.pos() && a.rhs() == b.rhs()) ||
           (a.lhs() == b.lhs() && !a.pos() && !b.pos() && a.rhs().name() && b.rhs().name() && a.rhs() != b.rhs());
  }

  // Complementary(a, b) holds when a, b match one of the following:
  // (t1 = t2), (t1 != t2)
  // (t1 != t2), (t1 = t2)
  // (t = n1), (t = n2) for distinct n1, n2.
  static bool Complementary(const Literal a, const Literal b) {
    assert(a.primitive());
    assert(b.primitive());
    return (a.lhs() == b.lhs() && a.pos() != b.pos() && a.rhs() == b.rhs()) ||
           (a.lhs() == b.lhs() && a.pos() && b.pos() && a.rhs().name() && b.rhs().name() && a.rhs() != b.rhs());
  }

  // ProperlySubsumes(a, b) holds when a is (t1 = n1) and b is (t1 != n2) for distinct n1, n2.
  static bool ProperlySubsumes(Literal a, Literal b) {
    return a.lhs() == b.lhs() && a.pos() && !b.pos() && a.rhs().name() && b.rhs().name() && a.rhs() != b.rhs();
  }

  static bool Subsumes(Literal a, Literal b) { return a == b || ProperlySubsumes(a, b); }

  bool Subsumes(Literal b) const { return Subsumes(*this, b); }

  bool ProperlySubsumes(Literal b) const { return ProperlySubsumes(*this, b); }

  template<typename UnaryFunction>
  Literal Substitute(UnaryFunction theta, Term::Factory* tf) const {
    return Literal(pos(), lhs().Substitute(theta, tf), rhs().Substitute(theta, tf));
  }

  template<Term::UnificationConfiguration config = Term::kDefaultConfig>
  static Maybe<Term::Substitution> Unify(Literal a, Literal b) {
    Term::Substitution sub;
    bool ok = Term::Unify<config>(a.lhs(), b.lhs(), &sub) &&
              Term::Unify<config>(a.rhs(), b.rhs(), &sub);
    return ok ? internal::Just(sub) : internal::Nothing;
  }

  static Maybe<Term::Substitution> Isomorphic(Literal a, Literal b) {
    Term::Substitution sub;
    bool ok = Term::Isomorphic(a.lhs(), b.lhs(), &sub);
    if (ok) {
      if (a.rhs() == b.rhs()) {
        sub.Add(a.rhs(), b.rhs());
        ok = true;
      } else {
        const Maybe<Term> ar = sub(a.rhs());
        ok = ar && ar == sub(b.rhs());
      }
    }
    return ok ? internal::Just(sub) : internal::Nothing;
  }

  template<typename UnaryFunction>
  void Traverse(UnaryFunction f) const {
    lhs().Traverse(f);
    rhs().Traverse(f);
  }

 private:
  typedef internal::u32 u32;
  typedef internal::u64 u64;

  explicit Literal(Term lhs) {
    // Shall be the operator<-minimum of all Literals with lhs.
    data_ = static_cast<u64>(lhs.id());
    assert(this->lhs() == lhs);
    assert(this->rhs().null());
    assert(!this->pos());
  }

  Literal(bool pos, Term lhs, Term rhs) {
    assert(!lhs.null());
    assert(!rhs.null());
    if (!(lhs < rhs)) {
      Term tmp = lhs;
      lhs = rhs;
      rhs = tmp;
    }
    if ((!lhs.function() && rhs.function()) || rhs.quasi_primitive()) {
      Term tmp = lhs;
      lhs = rhs;
      rhs = tmp;
    }
    assert(!rhs.function() || lhs.function());
    data_ = static_cast<u64>(lhs.id()) |
           (static_cast<u64>(pos) << 63) |
           (static_cast<u64>(rhs.id()) << 32);
    assert(this->lhs() == lhs);
    assert(this->rhs() == rhs);
    assert(this->pos() == pos);
  }

  u64 data_;
};

struct Literal::LhsHash {
  internal::hash32_t operator()(const Literal a) const { return a.lhs().hash(); }
};

}  // namespace limbo


namespace std {

template<>
struct hash<limbo::Literal> {
  limbo::internal::hash32_t operator()(const limbo::Literal a) const { return a.hash(); }
};

template<>
struct equal_to<limbo::Literal> {
  bool operator()(const limbo::Literal a, const limbo::Literal b) const { return a == b; }
};

}  // namespace std

#endif  // LIMBO_LITERAL_H_

