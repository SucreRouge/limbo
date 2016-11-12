// vim:filetype=cpp:textwidth=80:shiftwidth=2:softtabstop=2:expandtab
// Copyright 2014--2016 Christoph Schwering
//
// A couple of iterators to immitate Haskell lists with iterators.
//
// Maybe boost provides the same iterators and we should move to boost (this set
// of iterators evolved somewhat).

#ifndef SRC_ITER_H_
#define SRC_ITER_H_

#include <iterator>
#include <type_traits>

namespace lela {

// Iterates over numbers offset() + 0, offset() + 1, offset() + 2, ...
template<typename NullaryFunction>
struct incr_iterator {
 public:
  typedef std::ptrdiff_t difference_type;
  typedef typename std::result_of<NullaryFunction()>::type value_type;
  typedef value_type* pointer;
  typedef value_type& reference;
  typedef std::input_iterator_tag iterator_category;

  incr_iterator() = default;
  explicit incr_iterator(NullaryFunction offset) : offset_(offset), index_(0) {}

  bool operator==(incr_iterator it) const { return *(*this) == *it; }
  bool operator!=(incr_iterator it) const { return !(*this == it); }

  value_type operator*() const { assert(index_ >= 0); return offset_() + index_; }

  incr_iterator& operator++() { ++index_; return *this; }

 private:
  NullaryFunction offset_;
  value_type index_;
};

// Expects an iterator pointing to containers and iterates over their elements.
template<typename ContIter>
struct nested_iterator {
 public:
  typedef std::ptrdiff_t difference_type;
  typedef typename ContIter::value_type container_type;
  typedef decltype(std::declval<container_type>().begin()) iterator_type;
  typedef typename iterator_type::value_type value_type;
  typedef typename iterator_type::pointer pointer;
  typedef typename iterator_type::reference reference;
  typedef std::input_iterator_tag iterator_category;

  nested_iterator() = default;
  explicit nested_iterator(ContIter cont_first, ContIter cont_last)
      : cont_first_(cont_first),
        cont_last_(cont_last) {
    if (cont_first_ != cont_last_) {
      iter_ = (*cont_first_).begin();
    }
    Skip();
  }

  bool operator==(nested_iterator it) const {
    return cont_first_ == it.cont_first_ && (cont_first_ == cont_last_ || iter_ == it.iter_);
  }
  bool operator!=(nested_iterator it) const { return !(*this == it); }

  reference operator*() const {
    assert(cont_first_ != cont_last_);
    assert((*cont_first_).begin() != (*cont_first_).end());
    return *iter_;
  }

  nested_iterator& operator++() {
    assert(cont_first_ != cont_last_ && iter_ != (*cont_first_).end());
    ++iter_;
    Skip();
    return *this;
  }

 private:
  void Skip() {
    for (;;) {
      if (cont_first_ == cont_last_) {
        break;  // iterator has ended
      }
      if (iter_ != (*cont_first_).end()) {
        break;  // found next element
      }
      ++cont_first_;
      if (cont_first_ != cont_last_) {
        iter_ = (*cont_first_).begin();
      }
    }
    assert(cont_first_ == cont_last_ || iter_ != (*cont_first_).end());
  }

  ContIter cont_first_;
  const ContIter cont_last_;
  iterator_type iter_;
};

// Haskell's map function.
template<typename UnaryFunction, typename Iter>
struct transform_iterator {
 public:
  typedef std::ptrdiff_t difference_type;
  typedef typename std::result_of<UnaryFunction(typename Iter::value_type)>::type value_type;
  typedef value_type* pointer;
  typedef value_type& reference;
  typedef std::input_iterator_tag iterator_category;

  transform_iterator() = default;
  transform_iterator(UnaryFunction func, Iter iter) : func_(func), iter_(iter) {}

  bool operator==(transform_iterator it) const { return iter_ == it.iter_; }
  bool operator!=(transform_iterator it) const { return !(*this == it); }

  value_type operator*() const { return func_(*iter_); }

  transform_iterator& operator++() { ++iter_; return *this; }

 private:
  UnaryFunction func_;
  Iter iter_;
};

template<typename UnaryFunction, typename Iter>
struct transformed_range {
 public:
  typedef transform_iterator<UnaryFunction, Iter> iterator;

  transformed_range(UnaryFunction func, Iter begin, Iter end) : begin_(func, begin), end_(func, end) {}

  iterator begin() const { return begin_; }
  iterator end()   const { return end_; }

 private:
  iterator begin_;
  iterator end_;
};

template<typename UnaryFunction, typename Iter>
transformed_range<UnaryFunction, Iter> transform_range(UnaryFunction func, Iter begin, Iter end) {
  return transformed_range<UnaryFunction, Iter>(func, begin, end);
}

// Haskell's filter function.
template<typename UnaryPredicate, typename Iter>
struct filter_iterator {
 public:
  typedef std::ptrdiff_t difference_type;
  typedef typename Iter::value_type value_type;
  typedef value_type* pointer;
  typedef value_type& reference;
  typedef std::input_iterator_tag iterator_category;

  filter_iterator() = default;
  filter_iterator(UnaryPredicate pred, Iter it, const Iter end) : pred_(pred), iter_(it), end_(end) { Skip(); }

  bool operator==(filter_iterator it) const { return iter_ == it.iter_; }
  bool operator!=(filter_iterator it) const { return !(*this == it); }

  value_type operator*() const { return *iter_; }

  filter_iterator& operator++() { ++iter_; Skip(); return *this; }

 private:
  void Skip() {
    while (iter_ != end_ && !pred_(*iter_)) {
      ++iter_;
    }
  }

  UnaryPredicate pred_;
  mutable Iter iter_;
  const Iter end_;
};

template<typename UnaryPredicate, typename Iter>
struct filtered_range {
 public:
  typedef filter_iterator<UnaryPredicate, Iter> iterator;

  filtered_range(UnaryPredicate pred, Iter begin, Iter end) : begin_(pred, begin, end), end_(pred, end, end) {}

  iterator begin() const { return begin_; }
  iterator end()   const { return end_; }

 private:
  iterator begin_;
  iterator end_;
};

template<typename UnaryPredicate, typename Iter>
filtered_range<UnaryPredicate, Iter> filter_range(UnaryPredicate pred, Iter begin, Iter end) {
  return filtered_range<UnaryPredicate, Iter>(pred, begin, end);
}

template<typename Iter1, typename Iter2>
struct joined_ranges {
 public:
  struct iterator {
    typedef std::ptrdiff_t difference_type;
    typedef typename Iter1::value_type value_type;
    typedef typename Iter1::pointer pointer;
    typedef typename Iter1::reference reference;
    typedef std::input_iterator_tag iterator_category;

    iterator() = default;
    iterator(Iter1 it1, Iter1 end1, Iter2 it2) : it1_(it1), end1_(end1), it2_(it2) {}

    bool operator==(const iterator& it) const { return it1_ == it.it1_ && it2_ == it.it2_; }
    bool operator!=(const iterator& it) const { return !(*this == it); }

    reference operator*() const { return it1_ != end1_ ? *it1_ : *it2_; }

    iterator& operator++() {
      if (it1_ != end1_)  ++it1_;
      else                ++it2_;
      return *this;
    }

   private:
    Iter1 it1_;
    Iter1 end1_;
    Iter2 it2_;
  };

  joined_ranges(Iter1 begin1, Iter1 end1, Iter2 begin2, Iter2 end2)
      : begin1_(begin1), end1_(end1), begin2_(begin2), end2_(end2) {}

  iterator begin() const { return iterator(begin1_, end1_, begin2_); }
  iterator end()   const { return iterator(end1_, end1_, end2_); }

 private:
  Iter1 begin1_;
  Iter1 end1_;
  Iter2 begin2_;
  Iter2 end2_;
};

template<typename Iter1, typename Iter2>
inline joined_ranges<Iter1, Iter2> join_ranges(Iter1 begin1, Iter1 end1, Iter2 begin2, Iter2 end2) {
  return joined_ranges<Iter1, Iter2>(begin1, end1, begin2, end2);
}

}  // namespace lela

#endif  // SRC_ITER_H_

