#pragma once

#include <climits>

namespace WorldMap {

struct Spot {
    int index = -1;
    int mapId = 0;
};

namespace detail {
constexpr int OFF_SPOTS = 366;
constexpr int STRIDE = 17;
constexpr int X = 0;
constexpr int Y = 1;
constexpr int FIELD_ARR = 11;
constexpr int ORIGIN_X = 13;
constexpr int ORIGIN_Y = 35;
constexpr int HIT_RADIUS = 8;

inline int* GetPtr(const void* base, int idx) {
    return *reinterpret_cast<int* const*>(reinterpret_cast<const char*>(base) + idx * 4);
}
} // namespace detail

inline Spot GetSpot(void* base, int rx, int ry) {
    Spot out;
    int* spots = detail::GetPtr(base, detail::OFF_SPOTS);
    if (!spots) {
        return out;
    }

    int count = *(spots - 1);
    if (count <= 0) {
        return out;
    }

    int cx = rx - detail::ORIGIN_X;
    int cy = ry - detail::ORIGIN_Y;
    int best = -1;
    int bestDist = INT_MAX;

    for (int i = 0; i < count; ++i) {
        int* spot = spots + i * detail::STRIDE;
        unsigned* fields = *reinterpret_cast<unsigned**>(spot + detail::FIELD_ARR);
        if (!fields || *(fields - 1) == 0) {
            continue;
        }

        int dx = spot[detail::X] - cx;
        int dy = spot[detail::Y] - cy;
        int dist = dx * dx + dy * dy;

        if (dist <= detail::HIT_RADIUS * detail::HIT_RADIUS && dist < bestDist) {
            best = i;
            bestDist = dist;
        }
    }

    if (best < 0) {
        return out;
    }

    int* spot = spots + best * detail::STRIDE;
    unsigned* fields = *reinterpret_cast<unsigned**>(spot + detail::FIELD_ARR);

    out.index = best;
    out.mapId = (fields && *(fields - 1) > 0) ? static_cast<int>(fields[0]) : 0;
    return out;
}

} // namespace WorldMap
