#pragma once

#include <spdlog/spdlog.h>

#define LOGI(...) spdlog::info(__VA_ARGS__)
#define LOGE(...) spdlog::error(__VA_ARGS__)
