// vim:filetype=cpp:textwidth=120:shiftwidth=2:softtabstop=2:expandtab
// Copyright 2014 Christoph Schwering

#include <gtest/gtest.h>

#include <limbo/literal.h>
#include <limbo/format/output.h>

namespace limbo {

using namespace limbo::format;

TEST(LiteralTest, general) {
  Symbol::Factory& sf = *Symbol::Factory::Instance();
  Term::Factory& tf = *Term::Factory::Instance();
  const Symbol::Sort s1 = sf.CreateNonrigidSort();
  const Symbol::Sort s2 = sf.CreateNonrigidSort();
  const Term n1 = tf.CreateTerm(Symbol::Factory::CreateName(1, s1));
  const Term n2 = tf.CreateTerm(Symbol::Factory::CreateName(2, s1));
  const Term x1 = tf.CreateTerm(Symbol::Factory::CreateVariable(1, s1));
  const Term x2 = tf.CreateTerm(Symbol::Factory::CreateVariable(2, s1));
  const Term f1 = tf.CreateTerm(Symbol::Factory::CreateFunction(1, s1, 1), {n1});
  const Term f2 = tf.CreateTerm(Symbol::Factory::CreateFunction(2, s2, 2), {n1,x2});
  const Term f3 = tf.CreateTerm(Symbol::Factory::CreateFunction(1, s2, 1), {f1});
  const Term f4 = tf.CreateTerm(Symbol::Factory::CreateFunction(2, s2, 2), {n1,f1});

  EXPECT_TRUE(Literal::Eq(x1,n1).dual() == Literal::Eq(n1,x1));
  EXPECT_TRUE(Literal::Eq(x1,n1).flip() == Literal::Neq(x1,n1));
  EXPECT_TRUE(Literal::Eq(x1,n1).flip() == Literal::Neq(x1,n1).flip().flip());
  EXPECT_TRUE(Literal::Eq(x1,n1) == Literal::Eq(x1,n1).flip().flip());

  EXPECT_TRUE(!Literal::Eq(x1,n1).ground());
  EXPECT_TRUE(!Literal::Eq(x1,n1).primitive());
  EXPECT_TRUE(!Literal::Eq(x1,n1).quasi_primitive());
  EXPECT_TRUE(!Literal::Eq(x1,n1).flip().quasi_primitive());
  EXPECT_TRUE(!Literal::Eq(x1,n1).dual().quasi_primitive());

  EXPECT_TRUE(!Literal::Eq(x1,x1).ground());
  EXPECT_TRUE(!Literal::Eq(x1,x1).primitive());
  EXPECT_TRUE(!Literal::Eq(x1,x1).quasi_primitive());
  EXPECT_TRUE(!Literal::Eq(x1,x1).flip().quasi_primitive());
  EXPECT_TRUE(!Literal::Eq(x1,x1).dual().quasi_primitive());

  EXPECT_TRUE(Literal::Eq(f1,n1).ground());
  EXPECT_TRUE(Literal::Eq(f1,n1).primitive());
  EXPECT_TRUE(Literal::Eq(f1,n1).quasi_primitive());
  EXPECT_TRUE(Literal::Eq(f1,n1).flip().quasi_primitive());
  EXPECT_TRUE(Literal::Eq(f1,n1).dual().quasi_primitive());
  EXPECT_TRUE(Literal::Eq(f1,n1).dual() == Literal::Eq(f1,n1).dual());

  EXPECT_TRUE(!Literal::Eq(f2,n1).ground());
  EXPECT_TRUE(!Literal::Eq(f2,n1).primitive());
  EXPECT_TRUE(Literal::Eq(f2,n1).quasi_primitive());
  EXPECT_TRUE(Literal::Eq(f2,n1).flip().quasi_primitive());
  EXPECT_TRUE(Literal::Eq(f2,n1).dual().quasi_primitive());
  EXPECT_TRUE(Literal::Eq(f2,n1) == Literal::Eq(f2,n1).dual());

  EXPECT_TRUE(Literal::Eq(f3,n1).ground());
  EXPECT_TRUE(!Literal::Eq(f3,n1).primitive());
  EXPECT_TRUE(!Literal::Eq(f3,n1).quasi_primitive());
  EXPECT_TRUE(!Literal::Eq(f3,n1).flip().quasi_primitive());
  EXPECT_TRUE(!Literal::Eq(f3,n1).dual().quasi_primitive());
  EXPECT_TRUE(Literal::Eq(f3,n1).dual() == Literal::Eq(f3,n1).dual());

  EXPECT_TRUE(Literal::Eq(f4,n1).ground());
  EXPECT_TRUE(!Literal::Eq(f4,n1).primitive());
  EXPECT_TRUE(!Literal::Eq(f4,n1).quasi_primitive());
  EXPECT_TRUE(!Literal::Eq(f4,n1).flip().quasi_primitive());
  EXPECT_TRUE(!Literal::Eq(f4,n1).dual().quasi_primitive());
  EXPECT_TRUE(Literal::Eq(f4,n1) == Literal::Eq(f4,n1).dual());

  EXPECT_TRUE(Literal::Eq(n1,n1).valid());
  EXPECT_TRUE(!Literal::Neq(n1,n1).valid());
  EXPECT_TRUE(Literal::Eq(f1,f1).valid());
  EXPECT_TRUE(!Literal::Eq(f1,f2).valid());
  EXPECT_TRUE(!Literal::Neq(f1,f1).valid());
  EXPECT_TRUE(!Literal::Neq(f1,n1).valid());
  EXPECT_TRUE(Literal::Neq(f1,f2).valid());

  EXPECT_TRUE(!Literal::Eq(n1,n1).unsatisfiable());
  EXPECT_TRUE(Literal::Neq(n1,n1).unsatisfiable());
  EXPECT_TRUE(!Literal::Eq(f1,f1).unsatisfiable());
  EXPECT_TRUE(Literal::Eq(f1,f2).unsatisfiable());
  EXPECT_TRUE(Literal::Neq(f1,f1).unsatisfiable());
  EXPECT_TRUE(!Literal::Neq(f1,n1).unsatisfiable());
  EXPECT_TRUE(!Literal::Neq(f1,f2).unsatisfiable());

  EXPECT_TRUE(!Literal::Valid(Literal::Eq(f1, n1), Literal::Eq(f1, n1)));
  EXPECT_TRUE(!Literal::Valid(Literal::Eq(f1, n1), Literal::Neq(f1, n2)));
  EXPECT_TRUE(Literal::Valid(Literal::Eq(f1, n1), Literal::Neq(f1, n1)));
  EXPECT_TRUE(!Literal::Valid(Literal::Neq(f1, n1), Literal::Neq(f1, n1)));
  EXPECT_TRUE(!Literal::Valid(Literal::Eq(f1, n1), Literal::Eq(f1, n1)));
  EXPECT_TRUE(Literal::Valid(Literal::Neq(f1, n1), Literal::Neq(f1, n2)));
}

}  // namespace limbo

