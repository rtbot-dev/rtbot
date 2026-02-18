#include <catch2/catch.hpp>
#include <memory>

#include "rtbot/std/TopK.h"

using namespace rtbot;

SCENARIO("TopK maintains top-K entries by score (descending)", "[topk]") {
  SECTION("k=3 by price descending — roadmap test case") {
    auto topk = make_topk("t1", 3, 1, true);
    REQUIRE(topk->type_name() == "TopK");

    // Input [id, price]:
    // t=1: [1,100]  → top: [[1,100]]
    // t=2: [2,200]  → top: [[2,200],[1,100]]
    // t=3: [3,150]  → top: [[2,200],[3,150],[1,100]]
    // t=4: [4,50]   → top unchanged (50 < all top-3)
    // t=5: [5,180]  → top: [[2,200],[5,180],[3,150]] (evicted [1,100])

    auto send = [&](timestamp_t t, double id, double price) {
      topk->receive_data(
          create_message<VectorNumberData>(t, VectorNumberData{{id, price}}), 0);
    };

    send(1, 1, 100);
    send(2, 2, 200);
    send(3, 3, 150);
    send(4, 4, 50);
    send(5, 5, 180);
    topk->execute();

    auto& out = topk->get_output_queue(0);
    // t=1: 1 entry; t=2: 2; t=3: 3; t=4: 3 (re-emit); t=5: 3 → total 12
    REQUIRE(out.size() == 12);

    // Verify last 3 messages (timestamp=5): [2,200],[5,180],[3,150]
    auto* m0 = dynamic_cast<const Message<VectorNumberData>*>(out[9].get());
    auto* m1 = dynamic_cast<const Message<VectorNumberData>*>(out[10].get());
    auto* m2 = dynamic_cast<const Message<VectorNumberData>*>(out[11].get());
    REQUIRE(m0->time == 5);
    REQUIRE(m0->data.values[1] == Approx(200.0));
    REQUIRE(m1->data.values[1] == Approx(180.0));
    REQUIRE(m2->data.values[1] == Approx(150.0));

    // First message at t=1 should have [1,100]
    auto* first = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
    REQUIRE(first->time == 1);
    REQUIRE(first->data.values[0] == Approx(1.0));
    REQUIRE(first->data.values[1] == Approx(100.0));
  }

  SECTION("k=1 — single best entry") {
    auto topk = make_topk("t1", 1, 0, true);

    topk->receive_data(
        create_message<VectorNumberData>(1, VectorNumberData{{5.0}}), 0);
    topk->receive_data(
        create_message<VectorNumberData>(2, VectorNumberData{{3.0}}), 0);
    topk->receive_data(
        create_message<VectorNumberData>(3, VectorNumberData{{8.0}}), 0);
    topk->execute();

    auto& out = topk->get_output_queue(0);
    // t=1: emit [5]; t=2: emit [5] (3<5); t=3: emit [8]
    REQUIRE(out.size() == 3);

    auto* m0 = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
    REQUIRE(m0->data.values[0] == Approx(5.0));

    auto* m1 = dynamic_cast<const Message<VectorNumberData>*>(out[1].get());
    REQUIRE(m1->data.values[0] == Approx(5.0));  // 3 < 5, re-emit 5

    auto* m2 = dynamic_cast<const Message<VectorNumberData>*>(out[2].get());
    REQUIRE(m2->data.values[0] == Approx(8.0));
  }
}

SCENARIO("TopK ascending order", "[topk]") {
  SECTION("k=2 lowest values, score_index=0, descending=false") {
    auto topk = make_topk("t1", 2, 0, false);

    topk->receive_data(
        create_message<VectorNumberData>(1, VectorNumberData{{10.0, 1.0}}), 0);
    topk->receive_data(
        create_message<VectorNumberData>(2, VectorNumberData{{5.0, 2.0}}), 0);
    topk->receive_data(
        create_message<VectorNumberData>(3, VectorNumberData{{8.0, 3.0}}), 0);
    topk->receive_data(
        create_message<VectorNumberData>(4, VectorNumberData{{2.0, 4.0}}), 0);
    topk->execute();

    auto& out = topk->get_output_queue(0);
    // t=1: [10,1]; t=2: [5,2],[10,1]; t=3: [5,2],[8,3] (10 evicted); t=4: [2,4],[5,2] (8 evicted)
    REQUIRE(out.size() == 7);

    // Last 2 (at t=4): lowest by index 0 → [2,4] then [5,2]
    auto* last0 = dynamic_cast<const Message<VectorNumberData>*>(out[5].get());
    auto* last1 = dynamic_cast<const Message<VectorNumberData>*>(out[6].get());
    REQUIRE(last0->time == 4);
    REQUIRE(last0->data.values[0] == Approx(2.0));
    REQUIRE(last1->data.values[0] == Approx(5.0));
  }
}

SCENARIO("TopK serialization roundtrip", "[topk][State]") {
  SECTION("Collect and restore preserves top-K state") {
    auto topk = make_topk("t1", 2, 0, true);

    topk->receive_data(
        create_message<VectorNumberData>(1, VectorNumberData{{100.0}}), 0);
    topk->receive_data(
        create_message<VectorNumberData>(2, VectorNumberData{{200.0}}), 0);
    topk->execute();

    Bytes state = topk->collect();
    auto restored = make_topk("t1", 2, 0, true);
    auto it = state.cbegin();
    restored->restore(it);

    // Feed one more to the restored instance
    restored->clear_all_output_ports();
    restored->receive_data(
        create_message<VectorNumberData>(3, VectorNumberData{{150.0}}), 0);
    restored->execute();

    auto& out = restored->get_output_queue(0);
    REQUIRE(out.size() == 2);  // k=2: [200,150]
    auto* m0 = dynamic_cast<const Message<VectorNumberData>*>(out[0].get());
    auto* m1 = dynamic_cast<const Message<VectorNumberData>*>(out[1].get());
    REQUIRE(m0->data.values[0] == Approx(200.0));
    REQUIRE(m1->data.values[0] == Approx(150.0));
  }
}
