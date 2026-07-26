// built together with test_matching.cpp (that file has DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN),
// this file only contributes more TEST_CASEs.
#include "doctest.h"

#include "../memory_pool.h"
#include <stdexcept>


TEST_CASE("allocate returns non-null, distinct pointers") {
    MemoryPool pool(64, 4);

    void* a = pool.allocate();
    void* b = pool.allocate();

    CHECK(a != nullptr);
    CHECK(b != nullptr);
    CHECK(a != b);

    pool.deallocate(a);
    pool.deallocate(b);
}

TEST_CASE("deallocate followed by allocate returns the same block") {
    MemoryPool pool(64, 4);

    void* a = pool.allocate();
    pool.deallocate(a);
    void* b = pool.allocate();

    CHECK(a == b);
}

TEST_CASE("pool grows once the initial capacity is exhausted") {
    MemoryPool pool(64, 2);

    void* a = pool.allocate();
    void* b = pool.allocate();
    void* c = pool.allocate();   // beyond the initial 2 blocks -> grow()

    CHECK(a != nullptr);
    CHECK(b != nullptr);
    CHECK(c != nullptr);
    CHECK(a != b);
    CHECK(b != c);
    CHECK(a != c);

    pool.deallocate(a);
    pool.deallocate(b);
    pool.deallocate(c);
}

TEST_CASE("blockCount 0 is rejected at construction") {
    CHECK_THROWS_AS(MemoryPool(64, 0), std::invalid_argument);
}
