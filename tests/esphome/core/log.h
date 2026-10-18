#pragma once
template <class... Ts>
inline void test_log(Ts...) {}
#define ESP_LOGD(...) test_log(__VA_ARGS__)
#define ESP_LOGW(...) test_log(__VA_ARGS__)
#define ESP_LOGI(...) test_log(__VA_ARGS__)
