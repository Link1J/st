#pragma once

constexpr auto CLOCK_MONOTONIC = 0;

int clock_gettime(int X, struct timeval* tv);