// NumberField (#66): a count entry that cannot hold a non-count.
//
// The reason this is a control rather than validation at each call site is that the
// validation kept being re-derived and kept disagreeing. Everything below is about what
// the box refuses.
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "spry/spry.h"

using namespace spry;

TEST_CASE("a NumberField starts clamped into its range", "[numberfield]") {
    REQUIRE(NumberField(3, 0, 10).value() == 3);
    REQUIRE(NumberField(50, 0, 10).value() == 10); // above the ceiling
    REQUIRE(NumberField(-5, 2, 10).value() == 2);  // below the floor
}

TEST_CASE("a NumberField has no sign", "[numberfield]") {
    // The floor is floored: a caller asking for negatives is asking for a different
    // control, and silently accepting the request would hand them one that cannot type
    // a minus anyway.
    NumberField nf(0, -100, 10);
    REQUIRE(nf.minValue() == 0);
}

TEST_CASE("typing into a NumberField drops everything but digits", "[numberfield]") {
    NumberField nf(0, 0, 9999);
    nf.field().setText("");
    nf.field().onText("8");
    nf.field().onText(" workers");   // the string that started this
    nf.field().onText("-");
    nf.field().onText(".5");
    REQUIRE(nf.field().text() == "85"); // the 8, and the 5 out of ".5"
    REQUIRE(nf.value() == 85);
}

TEST_CASE("an empty NumberField is not zero", "[numberfield]") {
    // The distinction the scale dialog depends on: clearing the box must not read as a
    // request to scale to nothing.
    NumberField nf(3, 0, 9999);
    nf.field().setText("");
    REQUIRE_FALSE(nf.valid());
    REQUIRE(nf.value() == nf.minValue()); // a fallback, not an answer
}

TEST_CASE("stepping a NumberField stops at the bounds", "[numberfield]") {
    NumberField nf(1, 0, 3);
    nf.step(-1);
    REQUIRE(nf.value() == 0);
    nf.step(-1); // already at the floor
    REQUIRE(nf.value() == 0);
    nf.step(+1);
    nf.step(+1);
    nf.step(+1);
    REQUIRE(nf.value() == 3);
    nf.step(+1); // already at the ceiling
    REQUIRE(nf.value() == 3);
}

TEST_CASE("stepping out of an empty NumberField starts at the floor", "[numberfield]") {
    NumberField nf(7, 0, 99);
    nf.field().setText("");
    nf.step(+1);
    REQUIRE(nf.value() == 1); // from the floor, not from the 7 that was cleared
    REQUIRE(nf.valid());
}

TEST_CASE("a NumberField's step size is settable and never zero", "[numberfield]") {
    NumberField nf(0, 0, 100);
    nf.setStep(10);
    nf.step(+1);
    REQUIRE(nf.value() == 10);
    nf.setStep(0); // a zero step would make the arrows do nothing at all
    nf.step(+1);
    REQUIRE(nf.value() == 11);
}

TEST_CASE("a very long paste saturates rather than wrapping", "[numberfield]") {
    // value() accumulates digits; without the early bail a 20-digit paste would
    // overflow and could land anywhere, including below the floor.
    NumberField nf(0, 0, 9999);
    nf.field().setText("99999999999999999999");
    REQUIRE(nf.value() == 9999);
}

TEST_CASE("onValue reports the number, not the keystroke", "[numberfield]") {
    NumberField nf(0, 0, 99);
    int seen = -1;
    int calls = 0;
    nf.onValue = [&](int v) {
        seen = v;
        ++calls;
    };
    nf.field().setText("");
    nf.field().onText("4");
    REQUIRE(seen == 4);
    const int afterTyping = calls;
    nf.step(+1);
    REQUIRE(seen == 5);
    REQUIRE(calls > afterTyping);
}
