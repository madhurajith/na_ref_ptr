// Copyright (c) 2023 Namal Bambarasinghe
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include <na/ref_ptr.hpp>

#include <gtest/gtest.h>

TEST(na_ref_ptr_test_suit, referable_construction)
{
    struct a
    {
        int v;
    };

    a a1{42};
    na::referable<a> r1{a1};
    na::ref_ptr<a> p1 = r1;
    EXPECT_EQ(p1->v, 42);

    const a a2{23};
    na::referable<const a> r2{a2};
    na::ref_ptr<const a> p2 = r2;
    EXPECT_EQ(p2->v, 23);

    na::referable<a> r3{17};
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

    na::referable<b> r4(17, 43);
    na::ref_ptr<b> p4 = r4;
    EXPECT_EQ(p4->x, 17);
    EXPECT_EQ(p4->y, 43);
}

TEST(na_ref_ptr_test_suit, enable_ref_from_this_construction)
{
    struct a : na::enable_ref_from_this<a>
    {
        a(int i) : i{i} {}

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

    na::referable<a> r1{3, 5.0};
    na::ref_ptr<a> p1{r1};
    EXPECT_EQ(p1->i, 3);
    EXPECT_EQ(p1->d, 5.0);

    na::ref_ptr<int> p2{r1, &a::i};
    EXPECT_EQ(*p2, 3);

    na::ref_ptr<const double> p3{r1, &a::d};
    EXPECT_EQ(*p3, 5.0);
}
