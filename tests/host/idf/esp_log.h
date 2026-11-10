#pragma once
// Consume arguments so host warnings also check diagnostic expressions.
template <typename... T> void testLog(T...) {}
#define ESP_LOGE(...) testLog(__VA_ARGS__)
