// Copyright (c) 2023 Namal Bambarasinghe
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#define na_ref_ptr_counted
#define na_ref_ptr_test_suit na_ref_ptr_counted_tests
#include "all_tests.inl"

void make_referable_after_free_counted()
{
    na::ref_ptr<int> rp;
    {
        na::referable<int> r{1};
        rp = r;
    }
}

TEST(na_ref_ptr_test_suit, referable_after_free_test)
{
    bool referable_after_free_detected = false;
    na::set_referable_after_free_handler([&referable_after_free_detected](const std::string &msg) {
        referable_after_free_detected = true;
    });

    make_referable_after_free_counted();

    EXPECT_EQ(referable_after_free_detected, true);
}