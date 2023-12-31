// Copyright (c) 2023 Namal Bambarasinghe
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include <na/ref_ptr.hpp>

#include <gtest/gtest.h>

TEST(na_ref_ptr_test_suit, documentation_tests)
{
    // primitive type boxed in a referable
    na::referable<int> r = {1};
    int r1 = *r;
    na::ref_ptr<int> rp = r;
    int r2 = *rp;
    EXPECT_EQ(r1, r2);

#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
    EXPECT_EQ(rp.use_count(), 1);
#endif

    // user defined type boxed in a referable
    struct test
    {
        int a;
        float b;
    };

    na::referable<test> t = {{2, 5.0f}};

    // dereferencing the referable
    int ta = t->a;
    EXPECT_EQ(ta, 2);

    // dereferencing the ref_ptr
    na::ref_ptr<test> tp = t;
    float tb = tp->b;
    EXPECT_EQ(tb, 5.0f);

#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
    EXPECT_EQ(tp.use_count(), 1);
#endif
    // ref_ptr to a sub object
    na::ref_ptr<int> tp_a = {t, &test::a};
    EXPECT_EQ(*tp_a, 2);

#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
    EXPECT_EQ(tp.use_count(), 2);
#endif

    na::ref_ptr<float> tp_b = na::ref_ptr<float>{tp, &test::b};
    EXPECT_EQ(*tp_b, 5.0f);

#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
    EXPECT_EQ(tp.use_count(), 3);
#endif

    // allow safe references using enable_ref_from_this
    struct safely_referable_type : na::enable_ref_from_this<safely_referable_type>
    {
        safely_referable_type(double dd, const std::string &ss) : d(dd), s(ss)
        {
        }

        double d;
        std::string s;
    };

    safely_referable_type srt{3.0, "Hello"};
    na::ref_ptr<safely_referable_type> p = srt;

    double d = p->d;
    EXPECT_EQ(d, 3.0);

    std::string s = p->s;
    EXPECT_EQ(s, "Hello");

#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
    EXPECT_EQ(p.use_count(), 1);
#endif
}

TEST(na_ref_ptr_test_suit, referable_construction)
{
    struct a
    {
        int v;
    };

    struct d : a
    {
        float f;
    };

    a a1{42};
    na::referable<a> r1{a1};
    na::ref_ptr<a> p1 = r1;
    EXPECT_EQ(p1->v, 42);

    const a a2{23};
    na::referable<const a> r2{a2};
    na::ref_ptr<const a> p2 = r2;
    EXPECT_EQ(p2->v, 23);

    na::referable<a> r3{{17}};
    na::ref_ptr<a> p3 = r3;
    EXPECT_EQ(p3->v, 17);

    struct b
    {
      public:
        b(int x_in, int y_in) : x(x_in), y(y_in)
        {
        }

        int x, y;
    };

    na::referable<b> r4{{17, 43}};
    na::ref_ptr<b> p4 = r4;
    EXPECT_EQ(p4->x, 17);
    EXPECT_EQ(p4->y, 43);

    na::referable<d> r5{{{32}, 42.0f}};
    na::ref_ptr<d> p5 = r5;

    na::referable<a> r6 = r5;
    na::ref_ptr<a> p6 = r6;

#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
    EXPECT_EQ(p5.use_count(), 1);
    EXPECT_EQ(p6.use_count(), 1);
#endif

    na::referable<d> r7{{{55}, 65.0f}};
    r7 = r5;
    na::ref_ptr<d> p7 = r7;

#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
    EXPECT_EQ(p7.use_count(), 1);
#endif

    na::referable<a> r8{{223}};
    r8 = r5;
    na::ref_ptr<a> p8 = r8;

#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
    EXPECT_EQ(p8.use_count(), 1);
#endif

    p8 = p7;

#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
    EXPECT_EQ(p8.use_count(), 2);
#endif

    na::ref_ptr<a> p9 = r7;
    p9 = p7;

#if defined(na_ref_ptr_counted) || defined(na_ref_ptr_tracked)
    EXPECT_EQ(p9.use_count(), 3);
#endif
}

TEST(na_ref_ptr_test_suit, enable_ref_from_this_construction)
{
    struct a : na::enable_ref_from_this<a>
    {
        a(int i) : i{i}
        {
        }

        int i;
    };

    a a1{5};
    na::ref_ptr<a> p1 = a1;
    EXPECT_EQ(p1->i, 5);
}

TEST(na_ref_ptr_test_suit, ref_ptr_construction)
{
    struct a
    {
        int i;
        double d;
    };

    na::referable<a> r1{{3, 5.0}};
    na::ref_ptr<a> p1{r1};
    EXPECT_EQ(p1->i, 3);
    EXPECT_EQ(p1->d, 5.0);

    na::ref_ptr<int> p2{r1, &a::i};
    EXPECT_EQ(*p2, 3);

    na::ref_ptr<const double> p3{r1, &a::d};
    EXPECT_EQ(*p3, 5.0);
}
