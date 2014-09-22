% vim:filetype=prolog:textwidth=80:shiftwidth=4:softtabstop=4:expandtab
%
% Tool that generates a proper+ BAT represented in C code.
%
% The input BAT consists of Prolog clauses box/1, static/1, belief/1:
%
% For example, a sensing axiom can be written as
%   box('SF'(A) <-> A = sonar ^ (d1 v d0)).
% A successor state axiom can be written as
%   box(A:d0 <-> A = forward ^ d1 v d0).
%
% The operators `^', `v', `~' represent conjunction, disjunction, and negation.
% Implication and equivalence are written as `->' and `<->', and `true' can be
% used as well.
% The `box' indicates that the formula is surrounded by a Box operator in ES.
%
% The initial situation is axiomatized using `static' instead of `box':
%   static(d2 v d3).
%
% Belief conditionals as in 
%   belief(~d0 ^ ~d1 => d2 v d3).
% As in ESB, the `=>' is read counterfactually is no stand-alone logical
% operator but can only be used in connection as in belief(_ => _).
%
% Prolog variables in these Prolog clauses translate to ESBL variables. (Which
% are objects of type `var_t' in the generated C code. Their naming may differ,
% though, as they are called A, X, A1, X1, ...)
%
% Predicates and standard names (for actions or objects) need to be represented
% as Prolog terms which must be legal C identifiers as wells. For example,
% `'SF'' is the sensed fluent literal (which must be uppercase to conform with
% the ESBL constant defined in literal.h). Similarly, `forward' and `sonar'
% represent actions, and `d0', `d1', ... represent predicates. Observe that
% `d(0)' would not have worked, because it's no legal identifier in C.
%
% A BAT then consists of a bunch of Prolog clauses of the above format. Suppose
% a file `my-bat.pl' contains these clauses. Then the following command prints
% the BAT in proper+ form:
%   ?- print_all('bats/my-bat.pl').
% And this command generates the corresponding C code in the specified C header:
%   ?- compile_all('bats/my-bat.pl', 'my-generated-proper-plus-bat.h').
%
% It may improve performance to use different sorts for variables and standard
% names. For example, in the above sensed fluent axiom we can require `A' to be
% of sort `action':
%   box('SF'(A) <-> A = sonar ^ (d1 v d0)) :- put_sort(A, action).
% Then you need to adjust the disjunction in the generated C function
% `is_action': it must return true for only those standard names from the BAT
% which are of sort `action' and for those which have higher numbers than the
% names of the BAT (standard names of that kind will be used to deal with
% quantifiers).
%
% schwering@kbsg.rwth-aachen.de

:- use_module(library(apply)).

:- op(820, fx, ~).    /* Negation */
:- op(840, xfy, ^).   /* Conjunction */
:- op(850, xfy, v).   /* Disjunction */
:- op(870, xfy, ->).  /* Implication */
:- op(880, xfy, <->). /* Equivalence */
:- op(890, xfy, =>).  /* Belief conditional */

put_sort(V, Sort) :- put_attr(V, sort, Sort).
is_sort(V, Sort) :- get_attr(V, sort, Sort).

:- dynamic(box/1).
:- dynamic(static/1).
:- dynamic(belief/1).


cartesian([], _, []).
cartesian([A|N], L, M) :- pair(A, L, M1), cartesian(N, L, M2), append(M1, M2, M).

pair(_, [], []).
pair(A, [B|L], [AB|N]) :- append(A, B, AB), pair(A, L, N).

cnf(_ => _, _) :- !, throw('cnf/2: unexpected =>').
cnf(A1 <-> A2, B) :- !, cnf((A1 -> A2) ^ (A2 -> A1), B).
cnf(A1 -> A2, B) :- !, cnf(~A1 v A2, B).
cnf(A1 ^ A2, B) :- !, cnf(A1, B1), cnf(A2, B2), append(B1, B2, B).
cnf(A1 v A2, B) :- !, cnf(A1, B1), cnf(A2, B2), cartesian(B1, B2, B).
cnf(~(A1 ^ A2), B) :- !, cnf(~A1 v ~A2, B).
cnf(~(A1 v A2), B) :- !, cnf(~A1 ^ ~A2, B).
cnf(~(~A), B) :- !, cnf(A, B).
cnf(~true, [[]]) :- !.
cnf(~(X\=Y), [[X=Y]]) :- !.
cnf(~A, [[~A]]) :- !.
cnf(true, []) :- !.
cnf((X\=Y), [[~(X=Y)]]) :- !.
cnf(A, [[A]]).

var_list([], [], []).
var_list([V|Args], [V|NewArgs], E) :- var(V), !, var_list(Args, NewArgs, E).
var_list([T|Args], [V|NewArgs], [V=T|E]) :- var_list(Args, NewArgs, E).

varify(T1:(~T2), ~(T1:T3), E) :- !, varify(T2, T3, E).
varify(T1:T2, T1:T3, E) :- !, varify(T2, T3, E).
varify(~A, ~A1, E) :- !, varify(A, A1, E).
varify(A, A1, E) :- A =..[F|Args], var_list(Args, NewArgs, E), A1 =..[F|NewArgs].

ewffy([], [], []).
ewffy([(X=Y)|As], [~(X=Y)|Es], Ls) :- !, ewffy(As, Es, Ls).
ewffy([~(X=Y)|As], [(X=Y)|Es], Ls) :- !, ewffy(As, Es, Ls).
ewffy([(X\=Y)|As], [(X=Y)|Es], Ls) :- !, ewffy(As, Es, Ls).
ewffy([~(X\=Y)|As], [~(X=Y)|Es], Ls) :- !, ewffy(As, Es, Ls).
ewffy([A|As], Es, [L|Ls]) :- varify(A, L, Es1), ewffy(As, Es2, Ls), append(Es1, Es2, Es).

sortify([], []).
sortify([V|Vs], [sort(V, Sort)|E]) :- is_sort(V, Sort), !, sortify(Vs, E).
sortify([_|Vs], E) :- sortify(Vs, E).

sortify(E, C, E2) :-
    term_variables((E, C), Vars),
    sortify(Vars, E1),
    append(E1, E, E2).




variable_names([], _, []).
variable_names([V|Vs], I, [N=V|Ns]) :- is_sort(V, action), !, ( I = 0 -> N = 'A' ; atom_concat('A', I, N) ), I1 is I + 1, variable_names(Vs, I1, Ns).
variable_names([V|Vs], I, [N=V|Ns]) :- ( I = 0 -> N = 'X' ; atom_concat('X', I, N) ), I1 is I + 1, variable_names(Vs, I1, Ns).

extract_variable_names([], []).
extract_variable_names([N=_|Mapping], [N|Names]) :- extract_variable_names(Mapping, Names).

extract_standard_names([], []).
extract_standard_names([sort(_,_)|Es], Names) :-
    extract_standard_names(Es, Names).
extract_standard_names([~(X=Y)|Es], Names) :-
    extract_standard_names([(X=Y)|Es], Names).
extract_standard_names([(X=Y)|Es], Names) :-
    ( ground(X), ground(Y) ->
        Names = [X,Y|Names1]
    ; ground(X) ->
        Names = [X|Names1]
    ; ground(Y) ->
        Names = [Y|Names1]
    ;
        Names = Names1
    ),
    extract_standard_names(Es, Names1).

extract_sort_names([], []).
extract_sort_names([sort(_,Sort)|Es], [Sort|Names]) :-
    extract_sort_names(Es, Names).
extract_sort_names([~(_=_)|Es], Names) :-
    extract_sort_names(Es, Names).
extract_sort_names([(_=_)|Es], Names) :-
    extract_sort_names(Es, Names).

extract_predicate_names([], []).
extract_predicate_names([~L|Ls], Ps) :- !, extract_predicate_names([L|Ls], Ps).
extract_predicate_names([_:L|Ls], Ps) :- !, extract_predicate_names([L|Ls], Ps).
extract_predicate_names([L|Ls], [P|Ps]) :- functor(L, P, _), extract_predicate_names(Ls, Ps).

declarations(E, Cs, VarNames, StdNames, SortNames, PredNames) :-
    term_variables((E, Cs), Vars),
    variable_names(Vars, 0, Names),
    extract_variable_names(Names, VarNames),
    extract_standard_names(E, StdNames),
    extract_sort_names(E, SortNames),
    ( Cs = (C1, C2) ->
        extract_predicate_names(C1, PredNames1),
        extract_predicate_names(C2, PredNames2),
        append(PredNames1, PredNames2, PredNames)
    ;
        C = Cs,
        extract_predicate_names(C, PredNames)
    ).

compile_ewff([], 'TRUE').
compile_ewff([(X=Y)|E], 'AND'('EQ'(X, Y), E1)) :- compile_ewff(E, E1).
compile_ewff([~(X=Y)|E], 'AND'('NEQ'(X, Y), E1)) :- compile_ewff(E, E1).
compile_ewff([sort(X, Sort)|E], 'AND'('SORT'(X, Sort), E1)) :- compile_ewff(E, E1).

literal_z_f_a(~L, As, F, Ts) :- !, literal_z_f_a(L, As, F, Ts).
literal_z_f_a(A:L, [A|As], F, Ts) :- !, literal_z_f_a(L, As, F, Ts).
literal_z_f_a(L, [], F, Ts) :- L =..[F|Ts].

compile_literal(L, L_C) :-
    ( L  = (~_), L_C = 'N'(Actions, Symbol, Args)
    ; L \= (~_), L_C = 'P'(Actions, Symbol, Args)
    ),
    literal_z_f_a(L, ActionList, Symbol, ArgList),
    ( ActionList = [] -> Actions =..['Z', ''] ; Actions =..['Z'|ActionList] ),
    ( ArgList    = [] -> Args    =..['A', ''] ; Args    =..['A'|ArgList]    ).

compile_literals([], []).
compile_literals([L|Ls], [L_C|Ls_C]) :-
    compile_literal(L, L_C),
    compile_literals(Ls, Ls_C).

compile_clause(C, C_C) :-
    compile_literals(C, Ls_C),
    ( Ls_C = [] -> C_C =..['C', ''] ; C_C =..['C' | Ls_C] ).

compile_box(E, Alpha, Code) :-
    term_variables((E, Alpha), Vars),
    variable_names(Vars, 0, Names),
    compile_ewff(E, E_C),
    compile_clause(Alpha, Alpha_C),
    with_output_to(atom(Code), write_term('box_univ_clauses_append'('dynamic_bat', 'box_univ_clause_init'(E_C, Alpha_C)), [variable_names(Names)])).

compile_static(E, Alpha, Code) :-
    term_variables((E, Alpha), Vars),
    variable_names(Vars, 0, Names),
    compile_ewff(E, E_C),
    compile_clause(Alpha, Alpha_C),
    with_output_to(atom(Code), write_term('univ_clauses_append'('static_bat', 'univ_clause_init'(E_C, Alpha_C)), [variable_names(Names)])).

compile_belief(E, NegPhi, Psi, Code) :-
    term_variables((E, NegPhi, Psi), Vars),
    variable_names(Vars, 0, Names),
    compile_ewff(E, E_C),
    compile_clause(NegPhi, NegPhi_C),
    compile_clause(Psi, Psi_C),
    with_output_to(atom(Code), write_term('belief_conds_append'('belief_conds', 'belief_cond_init'(E_C, NegPhi_C, Psi_C)), [variable_names(Names)])).

compile(VarNames, StdNames, SortNames, PredNames, Code) :-
    box(Alpha),
    cnf(Alpha, Cnf),
    member(Clause, Cnf),
    ewffy(Clause, E1, C),
    sortify(E1, C, E),
    declarations(E, C, VarNames, StdNames, SortNames, PredNames),
    compile_box(E, C, Code).
compile(VarNames, StdNames, SortNames, PredNames, Code) :-
    static(Alpha),
    cnf(Alpha, Cnf),
    member(Clause, Cnf),
    ewffy(Clause, E1, C),
    sortify(E1, C, E),
    declarations(E, C, VarNames, StdNames, SortNames, PredNames),
    compile_static(E, C, Code).
compile(VarNames, StdNames, SortNames, PredNames, Code) :-
    belief(Phi => Psi),
    cnf(~Phi, NegPhiCnf),
    cnf(Psi, PsiCnf),
    member(NegPhiClause, NegPhiCnf),
    member(PsiClause, PsiCnf),
    ewffy(NegPhiClause, E11, C1),
    ewffy(PsiClause, E21, C2),
    sortify(E11, C1, E1),
    sortify(E21, C2, E2),
    append(E1, E2, E),
    declarations(E, (C1, C2), VarNames, StdNames, SortNames, PredNames),
    compile_belief(E, C1, C2, Code).

print_variable_name_declarations(_, []).
print_variable_name_declarations(Stream, [V|Vs]) :-
    length(Vs, I),
    format(Stream, 'static const var_t ~w = -1 * (~w + 1);~n', [V, I]),
    print_variable_name_declarations(Stream, Vs).

print_standard_name_declarations(_, []).
print_standard_name_declarations(Stream, [N|Ns]) :-
    length(Ns, I),
    format(Stream, 'static const stdname_t ~w = ~w + 1;~n', [N, I]),
    print_standard_name_declarations(Stream, Ns).

print_predicate_name_declarations(_, []).
print_predicate_name_declarations(Stream, [P|Ps]) :-
    length(Ps, I),
    ( P = 'SF' -> true ; format(Stream, 'static const pred_t ~w = ~w;~n', [P, I]) ),
    print_predicate_name_declarations(Stream, Ps).

print_clauses_definitions(_, []).
print_clauses_definitions(Stream, [C]) :- !, format(Stream, '    ~w;~n', [C]).
print_clauses_definitions(Stream, [C|Cs]) :- format(Stream, '    ~w;\\~n', [C]), print_clauses_definitions(Stream, Cs).

print_serialization(Stream, Name) :-
    format(Stream, '    else if (name == ~w) printf("~w");~n', [Name, Name]).

print_sort_names(_, [], _).
print_sort_names(Stream, [Sort|Sorts], StdNames) :-
    maplist(atom_concat(' || name == '), StdNames, DisjList),
    foldl(atom_concat, DisjList, '', Disj),
    maplist(atom_concat(' && name > '), StdNames, ConjList),
    foldl(atom_concat, ConjList, '', Conj),
    format(Stream, 'static bool is_~w(stdname_t name) {~n', [Sort]),
    format(Stream, '    return false~w || (true~w);~n', [Disj, Conj]),
    format(Stream, '}~n', []),
    format(Stream, '~n', []),
    print_sort_names(Stream, Sorts, StdNames).

print_functions(Stream, StdNames, SortNames, PredNames) :-
    format(Stream, 'static void print_stdname(stdname_t name) {~n', []),
    format(Stream, '    if (false) printf("never occurs");~n', []),
    maplist(print_serialization(Stream), StdNames),
    format(Stream, '    else printf("#%ld", name);~n', []),
    format(Stream, '}~n', []),
    format(Stream, '~n', []),
    format(Stream, 'static void print_pred(pred_t name) {~n', []),
    format(Stream, '    if (false) printf("never occurs");~n', []),
    maplist(print_serialization(Stream), PredNames),
    format(Stream, '    else printf("%d", name);~n', []),
    format(Stream, '}~n', []),
    format(Stream, '~n', []),
    print_sort_names(Stream, SortNames, StdNames).

compile_all(Input, Output) :-
    ( Output = stdout ->
        current_output(Stream)
    ;
        open(Output, write, Stream)
    ),
    retractall(box(_)),
    retractall(static(_)),
    retractall(belief(_)),
    consult(Input),
    findall(r(VarNames, StdNames, SortNames, PredNames, Code), compile(VarNames, StdNames, SortNames, PredNames, Code), All),
    maplist(arg(1), All, VarNames1),
    maplist(arg(2), All, StdNames1),
    maplist(arg(3), All, SortNames1),
    maplist(arg(4), All, PredNames1),
    maplist(arg(5), All, Code),
    flatten(VarNames1, VarNames2),
    flatten(StdNames1, StdNames2),
    flatten(SortNames1, SortNames2),
    flatten(PredNames1, PredNames2),
    sort(VarNames2, VarNames),
    sort(StdNames2, StdNames),
    sort(SortNames2, SortNames),
    sort(PredNames2, PredNames),
    format(Stream, '#include "bat-common.h"~n', []),
    format(Stream, '~n', []),
    print_variable_name_declarations(Stream, VarNames),
    format(Stream, '~n', []),
    print_standard_name_declarations(Stream, StdNames),
    format(Stream, '~n', []),
    print_predicate_name_declarations(Stream, PredNames),
    format(Stream, '~n', []),
    print_functions(Stream, StdNames, SortNames, PredNames),
    format(Stream, '#define DECL_ALL_CLAUSES(dynamic_bat, static_bat, belief_conds)\\~n', []),
    print_clauses_definitions(Stream, Code),
    format(Stream, '~n', []),
    ( Output = stdout ->
        true
    ;
        close(Stream)
    ).




cnf_to_ecnf([], []).
cnf_to_ecnf([C|Cs], ECs1) :- ewffy(C, E1, EC), sortify(E1, EC, E), ( (member((X1=Y1), E), member(~(X2=Y2), E), X1 == X2, Y1 == Y2) -> ECs1 = ECs ; ECs1 = [(E->EC)|ECs] ), cnf_to_ecnf(Cs, ECs).

print(ECnf) :-
    term_variables(ECnf, Vars),
    variable_names(Vars, 0, Names),
    write_term(ECnf, [variable_names(Names), nl(true)]), nl.

print_to_term('v', [], '~true').
print_to_term(_, '^', [], 'true').
print_to_term(Names, _, [X], T) :- !, with_output_to(atom(T), write_term(X, [variable_names(Names)])).
print_to_term(Names, Op, [X|Xs], T) :- print_to_term(Names, Op, Xs, Ts), with_output_to(atom(T1), write_term(X, [variable_names(Names)])), with_output_to(atom(T), format('~w ~w ~w', [T1, Op, Ts])).

print_all(Input) :-
    retractall(box(_)),
    retractall(static(_)),
    retractall(belief(_)),
    consult(Input),
    (   box(Alpha),
        print(box(Alpha)),
        cnf(Alpha, Cnf), member(Clause, Cnf), ewffy(Clause, E, C),
        term_variables(Alpha, Vars),
        variable_names(Vars, 0, Names),
        print_to_term(Names, '^', E, ETerm),
        print_to_term(Names, 'v', C, CTerm),
        Term = box(ETerm -> CTerm)
    ;   static(Alpha),
        print(Alpha),
        cnf(Alpha, Cnf), member(Clause, Cnf), ewffy(Clause, E, C),
        term_variables(Alpha, Vars),
        variable_names(Vars, 0, Names),
        print_to_term(Names, '^', E, ETerm),
        print_to_term(Names, 'v', C, CTerm),
        Term = (ETerm -> CTerm)
    ;   belief(Phi => Psi),
        print(Phi => Psi),
        cnf(~Phi, NegPhiCnf), cnf(Psi, PsiCnf), member(NegPhiClause, NegPhiCnf), member(PsiClause, PsiCnf), ewffy(NegPhiClause, E1, C1), ewffy(PsiClause, E2, C2), append(E1, E2, E),
        term_variables(Phi => Psi, Vars),
        variable_names(Vars, 0, Names),
        print_to_term(Names, '^', E, ETerm),
        print_to_term(Names, 'v', C1, CTerm1),
        print_to_term(Names, 'v', C2, CTerm2),
        Term = (ETerm -> (CTerm1 => CTerm2))
    ),
    write('  '),
    write_term(Term, [variable_names(Names), nl(true)]), nl,
    fail.
print_all(_).
