#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "cmap_data.h"

template <typename T>
T remap(T from_lo, T from_hi, T to_lo, T to_hi, T v)
{
    return std::lerp(to_lo, to_hi,
        std::clamp((v - from_lo) / (from_hi - from_lo), T { 0 }, T { 1 }));
}

template <typename T>
class Colormap {
    const std::vector<std::array<T, 3>>& cmap;
    T vmin = T{0};
    T vmax = T{1};

public:
    Colormap(const std::vector<std::array<T, 3>>& cmap)
        : cmap(cmap)
    {
    }

    void set_vrange(T new_vmin, T new_vmax) {
        vmin = new_vmin;
        vmax = new_vmax;
    }

    std::array<T, 3> operator()(T v) const
    {
        T v_scaled = remap(vmin, vmax, T{0}, static_cast<T>(cmap.size() - 1), v);
        std::size_t left = static_cast<int>(v_scaled);
        std::size_t right = left + 1;
        T frac = v_scaled - left;

        std::array<T, 3> result;
        std::transform(cmap[left].begin(), cmap[left].end(), cmap[right].begin(), result.begin(),
            [&](T l, T r) { return std::lerp(l, r, frac); });

        return result;
    }

    static Colormap by_name(const std::string& name)
    {
        if (name == "viridis") {
            return Colormap(viridis<T>);
        } else if (name == "inferno") {
            return Colormap(inferno<T>);
        } else if (name == "plasma") {
            return Colormap(plasma<T>);
        } else if (name == "magma") {
            return Colormap(magma<T>);
        } else if (name == "rocket") {
            return Colormap(rocket<T>);
        } else if (name == "mako") {
            return Colormap(mako<T>);
        }

        throw std::invalid_argument("Invalid colormap name");
    }
};
