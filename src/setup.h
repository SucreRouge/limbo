// vim:filetype=cpp:textwidth=80:shiftwidth=4:softtabstop=4:expandtab
/*
 * Setups are sets of ground clauses, which are disjunctions of (extended)
 * literals (as defined in literal.h).
 * Setups are the semantic primitive of ESL.
 * In ESL a setup is usually closed under unit propagation and subsumption,
 * and a setup entails a clause if it is contained in the setup.
 * Query answering is answered in queries.h.
 *
 * This header provides functions to generate setups from proper+ basic action
 * theories (BATs) and operations on setups, particularly subsumption test.
 * Proper+ BATs consist of implications where all variables are universally
 * quantified with maximum scope, the antecedent only is a so-called ewff, which
 * is a formula which mentions no other predicate than equality, and the
 * consequent is a disjunction of literals; formulas like successor state axioms
 * and sensed fluent axioms are boxed, that is, have a leading box operator
 * which says that they hold in any situation.
 * We don't close setups under subsumption to keep them finite; instead we check
 * if some clause in the setup subsumes the given one.
 *
 * Static formulas (for the initial situation) are expressed with
 * univ_clause_init(), while dynamic formulas (for preconditions, successor
 * state axioms, sensed fluent axioms) are expressed with
 * box_univ_clause_init().
 * Each of them consists of an ewff, which is a formula with only equality and
 * no other literals and all variables are implicitly universally quantified.
 * Variables are represented by negative values, standard names by non-negative
 * integers.
 *
 * bat_hplus() computes the set of standard name that needs to be considered
 * for quantification. These are the standard names from the BAT and the query
 * plus one additional standard name for each variable in the BAT and the query.
 *
 * setup_init_[static|dynamic]() create setups from the respective parts of a
 * BAT. It does so by substituting all variables with standard names from hplus
 * and, for the dynamic part, by instantiating the (implicit) box operators with
 * all prefixes of action sequences in the query, query_zs.
 * The returned setup is just the set containing all clauses resulting from the
 * ground -- no minimization or unit propagation has been done yet. Thus the
 * setup can be seen as the immediate result of grounding the clauses.
 * The setup_union() of the static and dynamic setups is perhaps what the user
 * is interested in. It is recommended to minimize that setup once.
 *
 * setup_minimize() removes all clauses subsumed by other clauses in the setup.
 * One the one hand, minimization may improve performance drastically. On the
 * other hand, it is itself quite expensive. It is hence recommonded to minimize
 * new setups and to avoid redundant minimizations.
 *
 * setup_propagate_units() closes the given setup under resolution of its unit
 * clauses with all other clauses. That is, for each resolvent of a unit clause
 * there is a clause in the new setup which subsumes this resolvent.
 * Moreoever, the returned setup is minimal if the given setup was minimal,
 * where minimality means that no clause is subsumed by another one in the
 * setup.
 *
 * setup_subsumes() returns true if unit propagation of the setup literals
 * contains a clause which subsumes the given clause, that is, is a subset of
 * the given clause.
 * Thus, setup_subsumes() is sound but not complete.
 *
 * setup_[inconsistent|entails]() indicate if inconsistency or a given clause,
 * respectively, can be proven for a given k. The parameter k denotes the number
 * splits, which means that the positive and negative case for k-many literals
 * is considered. In a way, k can be considered a search depth.
 * setup_[consistent|entails]() are equivalent to the semantics in the ESL paper
 * (Lakemeyer and Levesque, KR-2014) provided that the clause does not mention
 * SF literals (see below why).
 *
 * There are three differences between the implementation and the KR-2014 paper:
 *
 * Firstly, as splitting belongs to the territory of setups, it only takes place
 * while checking whether a clause is entailed. In contrast, the order of query
 * decomposition and splitting is nondeterministic in ESL. This is important
 * because for example proving the tautology p v q v (~p ^ ~q) depends on first
 * splitting p and q and only then decomposing the query. This needs is taken
 * care of by preprocessing the query appropriately in query.c (conversion to
 * CNF and grounding of quantifiers).
 *
 * Secondly, for each action in the query clause, the corresponding SF literal
 * is split during setup_entails(). These SF splits do not count towards the
 * split limit k.
 *
 * Thirdly, PEL (positive extended literals) of a setup and a query clause are
 * minimized in the implementation. Splitting forms a tree of depth k and
 * branching factor |PEL|. Reducing PEL is hence more efficient. We apply two
 * optimizations:
 * The initial PEL is the least set that contains
 *  (1) the atoms from the query clause, and
 *  (2) if an atom is in the PEL and mentioned in a clause from the setup, the
 *      PEL contains the atoms from that clause as well.
 * The idea here is that the setup may contain independent clauses (because, for
 * example, it may talk about totally unrelated issues), whose literals hence
 * will not help to prove the query.
 * The second optimization is that during splitting at level k, a non-SF literal
 * is considered relevant only if
 *  (1) there is no unit clause [a] or [~a] in the setup and
 *  (2) (a) c contains a or ~a or
 *      (b) some clause d from the setup contains a or ~a and
 *          (i)  |d| <= k+1, i.e., it might trigger unit propagation or
 *          (ii) |d\c| <= k, i.e., it might lead to subsumption.
 * (Depending on the size on the size of the setup, the second optimiziation may
 * be very expensive and perhaps should be skipped.)
 *
 * A consequence of the first PEL optimization (see above) is that our procedure
 * may not detect inconsistency through splitting. To conclusively show
 * inconsistency for a given k, one needs to consider all atoms from the setup
 * as PEL and try to prove the empty clause.
 *
 * To optimize consistency checks, we do it by the paper with the full PEL only
 * the first time for each given k. When new sensing results are added to the
 * setup through setup_add_sensing_result(), we update the memorized consistency
 * results as follows: If the setup was consistent before but becomes
 * inconsistent with the newly added SF literal, its negation would follow.
 * Hence it is enough to test setup_entails() for that negated SF literal, which
 * can again be shown for the minimized PEL and hence is relatively cheap.
 *
 * Notice that this optimization is based on the fact that the only safe way to
 * change the setup from the outside is setup_add_sensing_result().
 *
 * The first inconsistency check is performed on the first call of
 * setup_[inconsistent|entails](). As this needs the full PEL, it may be very
 * expensive; hence the user may use setup_guarantee_consistency() to skip the
 * first check. Subsequent setup_add_sensing_result() update the memorized
 * inconsistency results.
 *
 * To perform this inconsistency caching, our setup is not just a set of clauses
 * but also has a bitmap attribute.
 * Notice that the inconsistency checks and caching are performed in
 * setup_[inconsistent|entails](). Hence, it is safe to add or remove clauses
 * to the setup before the first call to these functions. (This is relevant for
 * belief.c.)
 *
 * schwering@kbsg.rwth-aachen.de
 */
#ifndef _SETUP_H_
#define _SETUP_H_

#include "bitmap.h"
#include "ewff.h"
#include "literal.h"
#include "set.h"
#include "term.h"

SET_DECL(clause, literal_t *);
SET_DECL(clauses, clause_t *);

typedef struct {
    clauses_t clauses;
    bitmap_t incons;
} setup_t;

typedef struct {
    const ewff_t *cond;
    const clause_t *clause;
    stdset_t names;
    varset_t vars;
} univ_clause_t;

typedef union { univ_clause_t c; } box_univ_clause_t;

VECTOR_DECL(univ_clauses, univ_clause_t *);
VECTOR_DECL(box_univ_clauses, box_univ_clause_t *);

clause_t clause_substitute(const clause_t *c, const varmap_t *map);

const univ_clause_t *univ_clause_init(
        const ewff_t *cond,
        const clause_t *clause);
const box_univ_clause_t *box_univ_clause_init(
        const ewff_t *cond,
        const clause_t *clause);

stdset_t bat_hplus(
        const univ_clauses_t *static_bat,
        const box_univ_clauses_t *dynamic_bat,
        const stdset_t *query_names,
        int n_query_vars);

setup_t setup_init_static(
        const univ_clauses_t *static_bat,
        const stdset_t *hplus);

setup_t setup_init_dynamic(
        const box_univ_clauses_t *dynamic_bat,
        const stdset_t *hplus,
        const stdvecset_t *query_zs);

setup_t setup_union(const setup_t *setup1, const setup_t *setup2);

setup_t setup_copy(const setup_t *setup);
setup_t setup_lazy_copy(const setup_t *setup);

int setup_cmp(const setup_t *setup1, const setup_t *setup2);

void setup_guarantee_consistency(setup_t *setup, const int k);

void setup_add_sensing_result(
        setup_t *setup,
        const stdvec_t *z,
        const stdname_t n,
        const bool r);

void setup_minimize(setup_t *setup);
void setup_propagate_units(setup_t *setup);

bool setup_subsumes(setup_t *setup, const clause_t *c);
bool setup_inconsistent(setup_t *setup, const int k);
bool setup_entails(setup_t *setup, const clause_t *c, const int k);

#endif

