#pragma once
template<typename... Types>
class Tuple {
public:
    Tuple() {};
};
template<
    typename T1>
class Tuple<T1> {
public:
    T1 Item1;
    Tuple() {};
    Tuple(T1 v1) :
        Item1(v1) {}
};
template<
    typename T1, typename T2>
class Tuple<T1, T2> {
public:
    T1 Item1; T2 Item2;
    Tuple() {};
    Tuple(T1 v1, T2 v2) :
        Item1(v1), Item2(v2) {}
};
template<
    typename T1, typename T2, typename T3>
class Tuple<T1, T2, T3> {
public:
    T1 Item1; T2 Item2; T3 Item3;
    Tuple() {};
    Tuple(T1 v1, T2 v2, T3 v3) :
        Item1(v1), Item2(v2), Item3(v3) {}
};
template<
    typename T1, typename T2, typename T3, typename T4>
class Tuple<T1, T2, T3, T4> {
public:
    T1 Item1; T2 Item2; T3 Item3; T4 Item4;
    Tuple() {};
    Tuple(T1 v1, T2 v2, T3 v3, T4 v4) :
        Item1(v1), Item2(v2), Item3(v3), Item4(v4) {}
};
template<
    typename T1, typename T2, typename T3, typename T4, typename T5>
class Tuple<T1, T2, T3, T4, T5> {
public:
    T1 Item1; T2 Item2; T3 Item3; T4 Item4; T5 Item5;
    Tuple() {};
    Tuple(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5) :
        Item1(v1), Item2(v2), Item3(v3), Item4(v4), Item5(v5) {}
};
template<
    typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
class Tuple<T1, T2, T3, T4, T5, T6> {
public:
    T1 Item1; T2 Item2; T3 Item3; T4 Item4; T5 Item5; T6 Item6;
    Tuple() {};
    Tuple(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6) :
        Item1(v1), Item2(v2), Item3(v3), Item4(v4), Item5(v5), Item6(v6) {}
};
template<
    typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
class Tuple<T1, T2, T3, T4, T5, T6, T7> {
public:
    T1 Item1; T2 Item2; T3 Item3; T4 Item4; T5 Item5; T6 Item6; T7 Item7;
    Tuple() {};
    Tuple(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7) :
        Item1(v1), Item2(v2), Item3(v3), Item4(v4), Item5(v5), Item6(v6), Item7(v7) {}
};
template<
    typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
class Tuple<T1, T2, T3, T4, T5, T6, T7, T8> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    Tuple() {};
    Tuple(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8) :
        Item1(v1), Item2(v2), Item3(v3), Item4(v4), Item5(v5), Item6(v6), Item7(v7), Item8(v8) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9>
class Tuple<T1, T2, T3, T4, T5, T6, T7, T8, T9> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;
    Tuple() {};
    Tuple(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8, T9 v9) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9, typename T10>
class Tuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10;
    Tuple() {};
    Tuple(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8, T9 v9, T10 v10) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9), Item10(v10) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9, typename T10, typename T11>
class Tuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11;
    Tuple() {};
    Tuple(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8, T9 v9, T10 v10, T11 v11) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9), Item10(v10), Item11(v11) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9, typename T10, typename T11, typename T12>
class Tuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12;
    Tuple() {};
    Tuple(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8, T9 v9, T10 v10, T11 v11, T12 v12) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9), Item10(v10), Item11(v11), Item12(v12) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9, typename T10, typename T11, typename T12, typename T13>
class Tuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13;
    Tuple() {};
    Tuple(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8, T9 v9, T10 v10, T11 v11, T12 v12, T13 v13) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9), Item10(v10), Item11(v11), Item12(v12), Item13(v13) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9, typename T10, typename T11, typename T12, typename T13, typename T14>
class Tuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14;
    Tuple() {};
    Tuple(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8, T9 v9, T10 v10, T11 v11, T12 v12, T13 v13, T14 v14) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9), Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9, typename T10, typename T11, typename T12, typename T13, typename T14, typename T15>
class Tuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15;
    Tuple() {};
    Tuple(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8, T9 v9, T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9), Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9, typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16>
class Tuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    Tuple() {};
    Tuple(
        T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6, T7 v7, T8 v8, T9 v9, T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9), Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17>
class Tuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18>
class Tuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19>
class Tuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20>
class Tuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21>
class Tuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22>
class Tuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23>
class Tuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24>
class Tuple<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24; T25 Item25;
    T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32; T33 Item33;
    T34 Item34; T35 Item35;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47, typename T48>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47, T48> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47; T48 Item48;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47, T48 v48) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47), Item48(v48) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47, typename T48,
    typename T49>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47, T48,
    T49> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47; T48 Item48;
    T49 Item49;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47, T48 v48,
        T49 v49) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47), Item48(v48),
        Item49(v49) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47, typename T48,
    typename T49, typename T50>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47, T48,
    T49, T50> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47; T48 Item48;
    T49 Item49; T50 Item50;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47, T48 v48,
        T49 v49, T50 v50) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47), Item48(v48),
        Item49(v49), Item50(v50) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47, typename T48,
    typename T49, typename T50, typename T51>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47, T48,
    T49, T50, T51> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47; T48 Item48;
    T49 Item49; T50 Item50; T51 Item51;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47, T48 v48,
        T49 v49, T50 v50, T51 v51) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47), Item48(v48),
        Item49(v49), Item50(v50), Item51(v51) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47, typename T48,
    typename T49, typename T50, typename T51, typename T52>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47, T48,
    T49, T50, T51, T52> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47; T48 Item48;
    T49 Item49; T50 Item50; T51 Item51; T52 Item52;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47, T48 v48,
        T49 v49, T50 v50, T51 v51, T52 v52) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47), Item48(v48),
        Item49(v49), Item50(v50), Item51(v51), Item52(v52) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47, typename T48,
    typename T49, typename T50, typename T51, typename T52, typename T53>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47, T48,
    T49, T50, T51, T52, T53> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47; T48 Item48;
    T49 Item49; T50 Item50; T51 Item51; T52 Item52; T53 Item53;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47, T48 v48,
        T49 v49, T50 v50, T51 v51, T52 v52, T53 v53) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47), Item48(v48),
        Item49(v49), Item50(v50), Item51(v51), Item52(v52), Item53(v53) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47, typename T48,
    typename T49, typename T50, typename T51, typename T52, typename T53, typename T54>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47, T48,
    T49, T50, T51, T52, T53, T54> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47; T48 Item48;
    T49 Item49; T50 Item50; T51 Item51; T52 Item52; T53 Item53; T54 Item54;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47, T48 v48,
        T49 v49, T50 v50, T51 v51, T52 v52, T53 v53, T54 v54) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47), Item48(v48),
        Item49(v49), Item50(v50), Item51(v51), Item52(v52), Item53(v53), Item54(v54) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47, typename T48,
    typename T49, typename T50, typename T51, typename T52, typename T53, typename T54, typename T55>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47, T48,
    T49, T50, T51, T52, T53, T54, T55> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47; T48 Item48;
    T49 Item49; T50 Item50; T51 Item51; T52 Item52; T53 Item53; T54 Item54; T55 Item55;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47, T48 v48,
        T49 v49, T50 v50, T51 v51, T52 v52, T53 v53, T54 v54, T55 v55) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47), Item48(v48),
        Item49(v49), Item50(v50), Item51(v51), Item52(v52), Item53(v53), Item54(v54), Item55(v55) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47, typename T48,
    typename T49, typename T50, typename T51, typename T52, typename T53, typename T54, typename T55, typename T56>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47, T48,
    T49, T50, T51, T52, T53, T54, T55, T56> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47; T48 Item48;
    T49 Item49; T50 Item50; T51 Item51; T52 Item52; T53 Item53; T54 Item54; T55 Item55; T56 Item56;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47, T48 v48,
        T49 v49, T50 v50, T51 v51, T52 v52, T53 v53, T54 v54, T55 v55, T56 v56) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47), Item48(v48),
        Item49(v49), Item50(v50), Item51(v51), Item52(v52), Item53(v53), Item54(v54), Item55(v55), Item56(v56) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47, typename T48,
    typename T49, typename T50, typename T51, typename T52, typename T53, typename T54, typename T55, typename T56,
    typename T57>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47, T48,
    T49, T50, T51, T52, T53, T54, T55, T56, T57> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47; T48 Item48;
    T49 Item49; T50 Item50; T51 Item51; T52 Item52; T53 Item53; T54 Item54; T55 Item55; T56 Item56;
    T57 Item57;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47, T48 v48,
        T49 v49, T50 v50, T51 v51, T52 v52, T53 v53, T54 v54, T55 v55, T56 v56, T57 v57) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47), Item48(v48),
        Item49(v49), Item50(v50), Item51(v51), Item52(v52), Item53(v53), Item54(v54), Item55(v55), Item56(v56),
        Item57(v57) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47, typename T48,
    typename T49, typename T50, typename T51, typename T52, typename T53, typename T54, typename T55, typename T56,
    typename T57, typename T58>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47, T48,
    T49, T50, T51, T52, T53, T54, T55, T56, T57, T58> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47; T48 Item48;
    T49 Item49; T50 Item50; T51 Item51; T52 Item52; T53 Item53; T54 Item54; T55 Item55; T56 Item56;
    T57 Item57; T58 Item58;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47, T48 v48,
        T49 v49, T50 v50, T51 v51, T52 v52, T53 v53, T54 v54, T55 v55, T56 v56, T57 v57, T58 v58) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47), Item48(v48),
        Item49(v49), Item50(v50), Item51(v51), Item52(v52), Item53(v53), Item54(v54), Item55(v55), Item56(v56),
        Item57(v57), Item58(v58) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47, typename T48,
    typename T49, typename T50, typename T51, typename T52, typename T53, typename T54, typename T55, typename T56,
    typename T57, typename T58, typename T59>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47, T48,
    T49, T50, T51, T52, T53, T54, T55, T56, T57, T58, T59> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47; T48 Item48;
    T49 Item49; T50 Item50; T51 Item51; T52 Item52; T53 Item53; T54 Item54; T55 Item55; T56 Item56;
    T57 Item57; T58 Item58; T59 Item59;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47, T48 v48,
        T49 v49, T50 v50, T51 v51, T52 v52, T53 v53, T54 v54, T55 v55, T56 v56, T57 v57, T58 v58, T59 v59) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47), Item48(v48),
        Item49(v49), Item50(v50), Item51(v51), Item52(v52), Item53(v53), Item54(v54), Item55(v55), Item56(v56),
        Item57(v57), Item58(v58), Item59(v59) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47, typename T48,
    typename T49, typename T50, typename T51, typename T52, typename T53, typename T54, typename T55, typename T56,
    typename T57, typename T58, typename T59, typename T60>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47, T48,
    T49, T50, T51, T52, T53, T54, T55, T56, T57, T58, T59, T60> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47; T48 Item48;
    T49 Item49; T50 Item50; T51 Item51; T52 Item52; T53 Item53; T54 Item54; T55 Item55; T56 Item56;
    T57 Item57; T58 Item58; T59 Item59; T60 Item60;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47, T48 v48,
        T49 v49, T50 v50, T51 v51, T52 v52, T53 v53, T54 v54, T55 v55, T56 v56, T57 v57, T58 v58, T59 v59, T60 v60) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47), Item48(v48),
        Item49(v49), Item50(v50), Item51(v51), Item52(v52), Item53(v53), Item54(v54), Item55(v55), Item56(v56),
        Item57(v57), Item58(v58), Item59(v59), Item60(v60) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47, typename T48,
    typename T49, typename T50, typename T51, typename T52, typename T53, typename T54, typename T55, typename T56,
    typename T57, typename T58, typename T59, typename T60, typename T61>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47, T48,
    T49, T50, T51, T52, T53, T54, T55, T56, T57, T58, T59, T60, T61> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47; T48 Item48;
    T49 Item49; T50 Item50; T51 Item51; T52 Item52; T53 Item53; T54 Item54; T55 Item55; T56 Item56;
    T57 Item57; T58 Item58; T59 Item59; T60 Item60; T61 Item61;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47, T48 v48,
        T49 v49, T50 v50, T51 v51, T52 v52, T53 v53, T54 v54, T55 v55, T56 v56, T57 v57, T58 v58, T59 v59, T60 v60, T61 v61) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47), Item48(v48),
        Item49(v49), Item50(v50), Item51(v51), Item52(v52), Item53(v53), Item54(v54), Item55(v55), Item56(v56),
        Item57(v57), Item58(v58), Item59(v59), Item60(v60), Item61(v61) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47, typename T48,
    typename T49, typename T50, typename T51, typename T52, typename T53, typename T54, typename T55, typename T56,
    typename T57, typename T58, typename T59, typename T60, typename T61, typename T62>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47, T48,
    T49, T50, T51, T52, T53, T54, T55, T56, T57, T58, T59, T60, T61, T62> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47; T48 Item48;
    T49 Item49; T50 Item50; T51 Item51; T52 Item52; T53 Item53; T54 Item54; T55 Item55; T56 Item56;
    T57 Item57; T58 Item58; T59 Item59; T60 Item60; T61 Item61; T62 Item62;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47, T48 v48,
        T49 v49, T50 v50, T51 v51, T52 v52, T53 v53, T54 v54, T55 v55, T56 v56, T57 v57, T58 v58, T59 v59, T60 v60, T61 v61, T62 v62) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47), Item48(v48),
        Item49(v49), Item50(v50), Item51(v51), Item52(v52), Item53(v53), Item54(v54), Item55(v55), Item56(v56),
        Item57(v57), Item58(v58), Item59(v59), Item60(v60), Item61(v61), Item62(v62) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47, typename T48,
    typename T49, typename T50, typename T51, typename T52, typename T53, typename T54, typename T55, typename T56,
    typename T57, typename T58, typename T59, typename T60, typename T61, typename T62, typename T63>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47, T48,
    T49, T50, T51, T52, T53, T54, T55, T56, T57, T58, T59, T60, T61, T62, T63> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47; T48 Item48;
    T49 Item49; T50 Item50; T51 Item51; T52 Item52; T53 Item53; T54 Item54; T55 Item55; T56 Item56;
    T57 Item57; T58 Item58; T59 Item59; T60 Item60; T61 Item61; T62 Item62; T63 Item63;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47, T48 v48,
        T49 v49, T50 v50, T51 v51, T52 v52, T53 v53, T54 v54, T55 v55, T56 v56, T57 v57, T58 v58, T59 v59, T60 v60, T61 v61, T62 v62, T63 v63) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47), Item48(v48),
        Item49(v49), Item50(v50), Item51(v51), Item52(v52), Item53(v53), Item54(v54), Item55(v55), Item56(v56),
        Item57(v57), Item58(v58), Item59(v59), Item60(v60), Item61(v61), Item62(v62), Item63(v63) {}
};
template<
    typename T1,  typename T2,  typename T3,  typename T4,  typename T5,  typename T6,  typename T7,  typename T8,
    typename T9,  typename T10, typename T11, typename T12, typename T13, typename T14, typename T15, typename T16,
    typename T17, typename T18, typename T19, typename T20, typename T21, typename T22, typename T23, typename T24,
    typename T25, typename T26, typename T27, typename T28, typename T29, typename T30, typename T31, typename T32,
    typename T33, typename T34, typename T35, typename T36, typename T37, typename T38, typename T39, typename T40,
    typename T41, typename T42, typename T43, typename T44, typename T45, typename T46, typename T47, typename T48,
    typename T49, typename T50, typename T51, typename T52, typename T53, typename T54, typename T55, typename T56,
    typename T57, typename T58, typename T59, typename T60, typename T61, typename T62, typename T63, typename T64>
class Tuple<
    T1,  T2,  T3,  T4,  T5,  T6,  T7,  T8,  T9,  T10, T11, T12, T13, T14, T15, T16, T17, T18, T19, T20, T21, T22, T23, T24,
    T25, T26, T27, T28, T29, T30, T31, T32, T33, T34, T35, T36, T37, T38, T39, T40, T41, T42, T43, T44, T45, T46, T47, T48,
    T49, T50, T51, T52, T53, T54, T55, T56, T57, T58, T59, T60, T61, T62, T63, T64> {
public:
    T1  Item1;  T2  Item2;  T3  Item3;  T4  Item4;  T5  Item5;  T6  Item6;  T7  Item7;  T8  Item8;
    T9  Item9;  T10 Item10; T11 Item11; T12 Item12; T13 Item13; T14 Item14; T15 Item15; T16 Item16;
    T17 Item17; T18 Item18; T19 Item19; T20 Item20; T21 Item21; T22 Item22; T23 Item23; T24 Item24;
    T25 Item25; T26 Item26; T27 Item27; T28 Item28; T29 Item29; T30 Item30; T31 Item31; T32 Item32;
    T33 Item33; T34 Item34; T35 Item35; T36 Item36; T37 Item37; T38 Item38; T39 Item39; T40 Item40;
    T41 Item41; T42 Item42; T43 Item43; T44 Item44; T45 Item45; T46 Item46; T47 Item47; T48 Item48;
    T49 Item49; T50 Item50; T51 Item51; T52 Item52; T53 Item53; T54 Item54; T55 Item55; T56 Item56;
    T57 Item57; T58 Item58; T59 Item59; T60 Item60; T61 Item61; T62 Item62; T63 Item63; T64 Item64;
    Tuple() {};
    Tuple(
        T1  v1,  T2  v2,  T3  v3,  T4  v4,  T5  v5,  T6  v6,  T7  v7,  T8  v8,  T9  v9,  T10 v10, T11 v11, T12 v12, T13 v13, T14 v14, T15 v15, T16 v16,
        T17 v17, T18 v18, T19 v19, T20 v20, T21 v21, T22 v22, T23 v23, T24 v24, T25 v25, T26 v26, T27 v27, T28 v28, T29 v29, T30 v30, T31 v31, T32 v32,
        T33 v33, T34 v34, T35 v35, T36 v36, T37 v37, T38 v38, T39 v39, T40 v40, T41 v41, T42 v42, T43 v43, T44 v44, T45 v45, T46 v46, T47 v47, T48 v48,
        T49 v49, T50 v50, T51 v51, T52 v52, T53 v53, T54 v54, T55 v55, T56 v56, T57 v57, T58 v58, T59 v59, T60 v60, T61 v61, T62 v62, T63 v63, T64 v64) :
        Item1(v1),   Item2(v2),   Item3(v3),   Item4(v4),   Item5(v5),   Item6(v6),   Item7(v7),   Item8(v8),
        Item9(v9),   Item10(v10), Item11(v11), Item12(v12), Item13(v13), Item14(v14), Item15(v15), Item16(v16),
        Item17(v17), Item18(v18), Item19(v19), Item20(v20), Item21(v21), Item22(v22), Item23(v23), Item24(v24),
        Item25(v25), Item26(v26), Item27(v27), Item28(v28), Item29(v29), Item30(v30), Item31(v31), Item32(v32),
        Item33(v33), Item34(v34), Item35(v35), Item36(v36), Item37(v37), Item38(v38), Item39(v39), Item40(v40),
        Item41(v41), Item42(v42), Item43(v43), Item44(v44), Item45(v45), Item46(v46), Item47(v47), Item48(v48),
        Item49(v49), Item50(v50), Item51(v51), Item52(v52), Item53(v53), Item54(v54), Item55(v55), Item56(v56),
        Item57(v57), Item58(v58), Item59(v59), Item60(v60), Item61(v61), Item62(v62), Item63(v63), Item64(v64) {}
};