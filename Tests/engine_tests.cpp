// Engine tests: plain C++, no JUCE, no framework. Run with ctest.
#include "Engine/Engine.h"

#include <cstdio>
#include <string>

static int g_checks = 0, g_failed = 0;
#define CHECK(...)                                                                     \
    do { ++g_checks; if (! (__VA_ARGS__)) { ++g_failed;                                 \
        std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #__VA_ARGS__); } } while (0)

static void testRoundTrip()
{
    engine::State a;
    a.gain = 0.25; a.mode = 2; a.uiScale = 1.5;
    engine::State b;
    CHECK (b.deserialize (a.serialize()));
    CHECK (b.serialize() == a.serialize());
}

static void testHostileInput()
{
    engine::State s;
    CHECK (! s.deserialize (""));
    CHECK (! s.deserialize ("random garbage"));
    CHECK (s.deserialize ("SolomonState v1\ngain=2147483647\nmode=-99\nuiscale=x\n"));
    CHECK (s.gain >= 0.0 && s.gain <= 1.0);
    CHECK (s.mode >= 0 && s.mode < engine::State::kNumModes);
    CHECK (s.uiScale >= 0.6 && s.uiScale <= 2.0);
    CHECK (! s.deserialize ("SolomonState v1\n" + std::string (engine::kMaxStateBytes, 'x')));
}

int main()
{
    testRoundTrip();
    testHostileInput();
    std::printf ("\n%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
