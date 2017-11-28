// vim:filetype=cpp:textwidth=120:shiftwidth=2:softtabstop=2:expandtab
// Copyright 2016-2017 Christoph Schwering
// Licensed under the MIT license. See LICENSE file in the project root.
//
// Symbols are the non-logical symbols of the language: variables, standard
// names, and function symbols, which are sorted. Symbols are immutable.
//
// Sorts can be assumed to be small integers, which makes them suitable to be
// used as Key in IntMaps. Sorts are immutable.
//
// Terms can be built from symbols as usual. Terms are immutable.
//
// The implementation aims to keep Terms as lightweight as possible to
// facilitate extremely fast copying and comparison. For that reason, Terms
// are interned and represented only with an index in the heap structure.
// Creating a Term a second time yields the same index.
//
// Using an index as opposed to a memory address gives us more control over how
// the representation of the Term looks like. In particular, it gets us the
// following advantages: fast yet deterministic (wrt multiple executions)
// hashing; smaller representation (31 bit); possibility to represent
// information in the index.
//
// Literal is a friend class of Term and builds on the memory layout of Term.
// In particular, exploits that Term::name() is encoded in Term::id(). That way
// certain operations on Terms and Literals can be expressed as bitwise
// operations on their integer representations.

#ifndef LIMBO_TERM_H_
#define LIMBO_TERM_H_

#include <cassert>

#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include <limbo/internal/hash.h>
#include <limbo/internal/intmap.h>
#include <limbo/internal/ints.h>
#include <limbo/internal/maybe.h>

namespace limbo {

template<typename T>
struct Singleton {
  static std::unique_ptr<T> instance;
};

template<typename T>
std::unique_ptr<T> Singleton<T>::instance;

class Symbol {
 public:
  typedef internal::u32 Id;
  typedef internal::u8 Arity;

  class Sort {
   public:
    typedef internal::u8 Id;

    static Sort Nonrigid(Id id) { return Sort(2 * id); }
    static Sort Rigid(Id id) { return Sort(2 * id + 1); }

    explicit Sort(Id id) : id_(id) {}
    explicit operator Id() const { return id_; }
    explicit operator internal::size_t() const { return id_; }

    bool operator==(const Sort s) const { return id_ == s.id_; }
    bool operator!=(const Sort s) const { return id_ != s.id_; }

    internal::hash32_t hash() const { return internal::jenkins_hash(id_); }

    Id id() const { return id_; }
    bool rigid() const { return id_ % 2 == 1; }

   private:
    Id id_;
  };

  class Factory : private Singleton<Factory> {
   public:
    static Factory* Instance() {
      if (instance == nullptr) {
        instance = std::unique_ptr<Factory>(new Factory());
      }
      return instance.get();
    }

    static void Reset() { instance = nullptr; }

    static Symbol CreateName(Id id, Sort sort) {
      assert(id > 0);
      return Symbol((id << 2) | 0, sort, 0);
    }

    static Symbol CreateVariable(Id id, Sort sort) {
      assert(id > 0);
      return Symbol((id << 2) | 1, sort, 0);
    }

    static Symbol CreateFunction(Id id, Sort sort, Arity arity) {
      assert(id > 0);
      assert(arity > 0 || !sort.rigid());
      return Symbol((id << 2) | 2, sort, arity);
    }

    Sort CreateNonrigidSort() { return Sort::Nonrigid(last_sort_++); }
    Sort CreateRigidSort()    { return Sort::Rigid(last_sort_++); }

    Symbol CreateName(Sort sort)                  { return CreateName(++last_name_, sort); }
    Symbol CreateVariable(Sort sort)              { return CreateVariable(++last_variable_, sort); }
    Symbol CreateFunction(Sort sort, Arity arity) { return CreateFunction(++last_function_, sort, arity); }

   private:
    Factory() = default;
    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;
    Factory(Factory&&) = delete;
    Factory& operator=(Factory&&) = delete;

    Sort::Id last_sort_ = 0;
    Id last_function_ = 0;
    Id last_name_ = 0;
    Id last_variable_ = 0;
  };

  Symbol() = default;

  bool operator==(Symbol s) const {
    assert(id_ != s.id_ || (sort_ == s.sort_ && arity_ == s.arity_));
    return id_ == s.id_;
  }
  bool operator!=(Symbol s) const { return !(*this == s); }

  internal::hash32_t hash() const { return internal::jenkins_hash(id_); }

  bool name()     const { return (id_ & (0 | 1 | 2)) == 0; }
  bool variable() const { return (id_ & (0 | 1 | 2)) == 1; }
  bool function() const { return (id_ & (0 | 1 | 2)) == 2; }

  bool null() const { return id_ == 0; }

  Id id() const { return id_ >> 2; }
  Sort sort() const { return sort_; }
  Arity arity() const { return arity_; }

 private:
  Symbol(Id id, Sort sort, Arity arity) : id_(id), sort_(sort), arity_(arity) {
    assert(sort.id() >= 0);
    assert(arity >= 0);
    assert(!function() || !variable() || arity == 0);
  }

  Id id_ = 0;
  Sort sort_ = Sort(0);
  Arity arity_ = 0;
};

class Term {
 public:
  typedef internal::size_t size_t;
  class Factory;
  struct Substitution;
  typedef std::vector<Term> Vector;  // using Vector within Term will be legal in C++17, but seems to be illegal before
  typedef internal::i8 UnificationConfiguration;
  template<typename T>
  using Maybe = internal::Maybe<T>;

  static constexpr UnificationConfiguration kUnifyLeft = (1 << 0);
  static constexpr UnificationConfiguration kUnifyRight = (1 << 1);
  static constexpr UnificationConfiguration kOccursCheck = (1 << 4);
  static constexpr UnificationConfiguration kUnifyTwoWay = kUnifyLeft | kUnifyRight;
  static constexpr UnificationConfiguration kDefaultConfig = kUnifyTwoWay;

  Term() = default;

  bool operator==(Term t) const { return id_ == t.id_; }
  bool operator!=(Term t) const { return id_ != t.id_; }
  bool operator<=(Term t) const { return id_ <= t.id_; }
  bool operator>=(Term t) const { return id_ >= t.id_; }
  bool operator<(Term t)  const { return id_ < t.id_; }
  bool operator>(Term t)  const { return id_ > t.id_; }

  internal::hash32_t hash() const { return internal::jenkins_hash(id_); }

  inline Symbol symbol()      const;
  inline const Vector& args() const;

  Symbol::Sort sort()   const { return symbol().sort(); }
  Symbol::Arity arity() const { return symbol().arity(); }
  Term arg(size_t i)    const { return args()[i]; }

  bool null()            const { return id_ == 0; }
  bool name()            const { return ((id_ & 1) == 1); }
  bool variable()        const { return symbol().variable(); }
  bool function()        const { return symbol().function(); }
  bool ground()          const { return name() || (function() && all_args<&Term::ground>()); }
  bool primitive()       const { return !sort().rigid() && function() && all_args<&Term::name>(); }
  bool quasi_name()      const { return !function() || (sort().rigid() && no_arg<&Term::function>()); }
  bool quasi_primitive() const { return !sort().rigid() && function() && all_args<&Term::quasi_name>(); }

  bool Mentions(Term t) const {
    return *this == t || std::any_of(args().begin(), args().end(), [t](Term tt) { return tt.Mentions(t); });
  }

  template<typename UnaryFunction>
  Term Substitute(UnaryFunction theta, Factory* tf) const;

  template<Term::UnificationConfiguration config = Term::kDefaultConfig>
  static bool Unify(Term l, Term r, Substitution* sub);
  template<Term::UnificationConfiguration config = Term::kDefaultConfig>
  static Maybe<Substitution> Unify(Term l, Term r);

  static bool Isomorphic(Term l, Term r, Substitution* sub);
  static Maybe<Substitution> Isomorphic(Term l, Term r);

  template<typename UnaryFunction>
  void Traverse(UnaryFunction f) const;

 private:
  friend class Literal;

  typedef internal::u32 u32;
  struct Data;

  explicit Term(u32 id) : id_(id) {}

  inline const Data* data() const;

  u32 id() const { return id_; }

  template<bool (Term::*Prop)() const>
  inline bool all_args() const {
    for (const Term t : args()) {
      if (!(t.*Prop)()) {
        return false;
      }
    }
    return true;
  }

  template<bool (Term::*Prop)() const>
  inline bool no_arg() const {
    for (const Term t : args()) {
      if ((t.*Prop)()) {
        return false;
      }
    }
    return true;
  }

  u32 id_ = 0;
};

struct Term::Data {
  Data(Symbol symbol, const Vector& args) : symbol(symbol), args(args) {}

  bool operator==(const Data& d) const { return symbol == d.symbol && args == d.args; }
  bool operator!=(const Data& s) const { return !(*this == s); }

  internal::hash32_t hash() const {
    internal::hash32_t h = symbol.hash();
    for (const limbo::Term t : args) {
      h ^= t.hash();
    }
    return h;
  }

  Symbol symbol;
  Vector args;
};

class Term::Factory : private Singleton<Factory> {
 public:
  static Factory* Instance() {
    if (instance == nullptr) {
      instance = std::unique_ptr<Factory>(new Factory());
    }
    return instance.get();
  }

  static void Reset() { instance = nullptr; }

  ~Factory() {
    for (Data* data : name_heap_) {
      delete data;
    }
    for (Data* data : variable_and_function_heap_) {
      delete data;
    }
  }

  Term CreateTerm(Symbol symbol) {
    return CreateTerm(symbol, {});
  }

  Term CreateTerm(Symbol symbol, const Vector& args) {
    assert(!symbol.null() && std::all_of(args.begin(), args.end(), [](const Term t) { return !t.null(); }));
    assert(symbol.arity() == static_cast<Symbol::Arity>(args.size()));
    Data* d = new Data(symbol, args);
    DataPtrSet* s = &memory_[symbol.sort()];
    auto it = s->find(d);
    if (it == s->end()) {
      const bool name = symbol.name() ||
          (symbol.sort().rigid() && symbol.function() &&
           std::all_of(args.begin(), args.end(), [](const Term t) { return t.name() && !t.function(); }));
      std::vector<Data*>* heap = name ? &name_heap_ : &variable_and_function_heap_;
      heap->push_back(d);
      const u32 id = (static_cast<u32>(heap->size()) << 1) | static_cast<u32>(name);
      s->insert(std::make_pair(d, id));
      return Term(id);
    } else {
      const u32 id = it->second;
      delete d;
      return Term(id);
    }
  }

  const Data* get(u32 id) const {
    if ((id & 1) == 1) {
      return name_heap_[(id >> 1) - 1];
    } else {
      return variable_and_function_heap_[(id >> 1) - 1];
    }
  }

 private:
  struct DataPtrHash { internal::hash32_t operator()(const Term::Data* d) const { return d->hash(); } };
  struct DataPtrEquals { bool operator()(const Term::Data* a, const Term::Data* b) const { return *a == *b; } };

  Factory() = default;
  Factory(const Factory&) = delete;
  Factory& operator=(const Factory&) = delete;
  Factory(Factory&&) = delete;
  Factory& operator=(Factory&&) = delete;

  typedef std::unordered_map<Data*, u32, DataPtrHash, DataPtrEquals> DataPtrSet;
  internal::IntMap<Symbol::Sort, DataPtrSet> memory_;
  std::vector<Data*> name_heap_;
  std::vector<Data*> variable_and_function_heap_;
};

struct Term::Substitution {
  Substitution() = default;
  Substitution(Term old, Term sub) { Add(old, sub); }

  bool Add(Term old, Term sub) {
    Maybe<Term> bef = operator()(old);
    if (!bef) {
      subs_.push_back(std::make_pair(old, sub));
      return true;
    } else {
      return bef.val == sub;
    }
  }

  Maybe<Term> operator()(const Term t) const {
    for (auto p : subs_) {
      if (p.first == t) {
        return internal::Just(p.second);
      }
    }
    return internal::Nothing;
  }

  std::vector<std::pair<Term, Term>> subs_;
};

inline Symbol Term::symbol()            const { return data()->symbol; }
inline const Term::Vector& Term::args() const { return data()->args; }
inline const Term::Data* Term::data()   const { return Factory::Instance()->get(id_); }

template<typename UnaryFunction>
Term Term::Substitute(UnaryFunction theta, Factory* tf) const {
  Maybe<Term> t = theta(*this);
  if (t) {
    return t.val;
  } else if (arity() > 0) {
    Vector args;
    args.reserve(data()->args.size());
    for (Term arg : data()->args) {
      args.push_back(arg.Substitute(theta, tf));
    }
    if (args != data()->args) {
      return tf->CreateTerm(data()->symbol, args);
    } else {
      return *this;
    }
  } else {
    return *this;
  }
}

template<Term::UnificationConfiguration config>
bool Term::Unify(Term l, Term r, Substitution* sub) {
  if (l == r) {
    return true;
  }
  Maybe<Term> u;
  l = (config & kUnifyLeft) != 0 && (u = (*sub)(l)) ? u.val : l;
  r = (config & kUnifyRight) != 0 && (u = (*sub)(r)) ? u.val : r;
  if (l.sort() != r.sort()) {
    return false;
  }
  if (l.symbol() == r.symbol()) {
    for (size_t i = 0; i < l.arity(); ++i) {
      if (!Unify<config>(l.arg(i), r.arg(i), sub)) {
        return false;
      }
    }
    return true;
  } else if (l.variable() && (config & kUnifyLeft) != 0 && sub->Add(l, r)) {
    return (config & kOccursCheck) == 0 || !r.Mentions(l);
  } else if (r.variable() && (config & kUnifyRight) != 0 && sub->Add(r, l)) {
    return (config & kOccursCheck) == 0 || !l.Mentions(r);
  } else {
    return false;
  }
}

template<Term::UnificationConfiguration config>
internal::Maybe<Term::Substitution> Term::Unify(Term l, Term r) {
  Substitution sub;
  return Unify<config>(l, r, &sub) ? internal::Just(sub) : internal::Nothing;
}

bool Term::Isomorphic(Term l, Term r, Substitution* sub) {
  if (l.function() && r.function() && !l.name() && !r.name() && l.symbol() == r.symbol()) {
    for (Symbol::Arity i = 0; i < l.arity(); ++i) {
      if (!Isomorphic(l.arg(i), r.arg(i), sub)) {
        return false;
      }
    }
    return true;
  } else if (l.variable() && r.variable() && l.sort() == r.sort() && sub->Add(l, r) && sub->Add(r, l)) {
    return true;
  } else if (l.name() && r.name() && l.sort() == r.sort() && sub->Add(l, r) && sub->Add(r, l)) {
    return true;
  } else {
    return false;
  }
}

internal::Maybe<Term::Substitution> Term::Isomorphic(Term l, Term r) {
  Substitution sub;
  return Isomorphic(l, r, &sub) ? internal::Just(sub) : internal::Nothing;
}

template<typename UnaryFunction>
void Term::Traverse(UnaryFunction f) const {
  if (f(*this) && arity() > 0) {
    for (Term arg : args()) {
      arg.Traverse(f);
    }
  }
}

}  // namespace limbo


namespace std {

template<>
struct hash<limbo::Symbol::Sort> {
  limbo::internal::hash32_t operator()(const limbo::Symbol::Sort s) const { return s.hash(); }
};

template<>
struct equal_to<limbo::Symbol::Sort> {
  bool operator()(const limbo::Symbol::Sort a, const limbo::Symbol::Sort b) const { return a == b; }
};

template<>
struct hash<limbo::Symbol> {
  limbo::internal::hash32_t operator()(const limbo::Symbol s) const { return s.hash(); }
};

template<>
struct equal_to<limbo::Symbol> {
  bool operator()(const limbo::Symbol a, const limbo::Symbol b) const { return a == b; }
};

template<>
struct hash<limbo::Term> {
  limbo::internal::hash32_t operator()(const limbo::Term t) const { return t.hash(); }
};

template<>
struct equal_to<limbo::Term> {
  bool operator()(const limbo::Term a, const limbo::Term b) const { return a == b; }
};

}  // namespace std

#endif  // LIMBO_TERM_H_

