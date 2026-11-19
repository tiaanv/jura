#pragma once
template <class... Ts>
inline void test_log(Ts...) {}
#ifdef TEST_NO_DEBUG_LOGGING
#define ESP_LOGD(...)
#else
#define ESP_LOGD(...) test_log(__VA_ARGS__)
#endif
#define ESP_LOGW(...) test_log(__VA_ARGS__)
#define ESP_LOGI(...) test_log(__VA_ARGS__)
