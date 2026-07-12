// wird zusammen mit test_matching.cpp gebaut (dort steckt DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN),
// diese Datei traegt nur weitere TEST_CASEs bei.
#include "doctest.h"

#include "../memory_pool.h"
#include <stdexcept>


TEST_CASE("allocate liefert nicht-null und unterschiedliche Zeiger") {
    MemoryPool pool(64, 4);

    void* a = pool.allocate();
    void* b = pool.allocate();

    CHECK(a != nullptr);
    CHECK(b != nullptr);
    CHECK(a != b);

    pool.deallocate(a);
    pool.deallocate(b);
}

TEST_CASE("deallocate gefolgt von allocate liefert denselben Block zurueck") {
    MemoryPool pool(64, 4);

    void* a = pool.allocate();
    pool.deallocate(a);
    void* b = pool.allocate();

    CHECK(a == b);
}

TEST_CASE("Pool waechst, wenn die initiale Kapazitaet aufgebraucht ist") {
    MemoryPool pool(64, 2);

    void* a = pool.allocate();
    void* b = pool.allocate();
    void* c = pool.allocate();   // ueber die initialen 2 Bloecke hinaus -> grow()

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

TEST_CASE("blockCount 0 wird bei Konstruktion abgelehnt") {
    CHECK_THROWS_AS(MemoryPool(64, 0), std::invalid_argument);
}
