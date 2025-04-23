#pragma once

#include <spdlog/spdlog.h>

#define LOGD(...) spdlog::debug(__VA_ARGS__)
#define LOGI(...) spdlog::info(__VA_ARGS__)
#define LOGW(...) spdlog::warn(__VA_ARGS__)
#define LOGE(...) spdlog::error(__VA_ARGS__)
