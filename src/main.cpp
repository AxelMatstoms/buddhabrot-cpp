#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <random>
#include <thread>
#include <vector>

#include "cmap.h"

std::vector<bool> binary_mandelbrot(std::uint64_t size, std::uint64_t max_iter)
{
    std::vector<bool> result(size * size);

    float delta = 4.0f / size;
    std::random_device rd;
    std::default_random_engine eng;

    std::uniform_real_distribution offset(-0.25f * delta, 0.25f * delta);

    for (std::uint64_t y = 0; y < size; ++y) {
        float im = std::lerp(-2.0f, 2.0f, static_cast<float>(y) / size);
        for (std::uint64_t x = 0; x < size; ++x) {
            float re = std::lerp(-2.0f, 2.0f, static_cast<float>(x) / size);

            std::complex<float> c { re + offset(eng), im + offset(eng) };
            std::complex<float> z {};

            for (std::uint64_t i = 0; i < max_iter && std::norm(z) < 4.0f; ++i) {
                z = z * z + c;
            }

            if (std::norm(z) < 4.0f) {
                result[y * size + x] = true;
            }
        }
    }

    return result;
}

std::vector<bool> im_edge(const std::vector<bool>& im, std::int64_t size)
{
    std::vector<bool> result(im.size());

    std::vector<std::int64_t> deltas { -size, -1, 1, size };
    auto in_bounds = [&](std::int64_t idx) { return idx >= 0 && idx < im.size(); };

    for (std::int64_t i = 0; i < im.size(); ++i) {
        result[i] = im[i] && std::any_of(deltas.begin(), deltas.end(), [&](std::int64_t delta) { return in_bounds(i + delta) && !im[i + delta]; });
    }

    return result;
}

std::vector<bool> im_invert(std::vector<bool> im)
{
    std::transform(im.begin(), im.end(), im.begin(), [](bool v) { return !v; });

    return im;
}

std::vector<bool> im_or(std::vector<bool> im1, const std::vector<bool>& im2)
{
    std::transform(im1.begin(), im1.end(), im2.begin(), im1.begin(), [](bool v1, bool v2) { return v1 || v2; });

    return im1;
}

std::vector<bool> im_dilate(const std::vector<bool>& im, std::int64_t size)
{
    std::vector<bool> result(im.size());

    std::vector<std::int64_t> deltas { -size, -1, 0, 1, size };
    auto in_bounds = [&](std::int64_t idx) { return idx >= 0 && idx < im.size(); };

    for (std::int64_t i = 0; i < im.size(); ++i) {
        result[i] = std::any_of(deltas.begin(), deltas.end(), [&](std::int64_t delta) { return in_bounds(i + delta) && im[i + delta]; });
    }

    return result;
}

std::vector<std::pair<float, float>> im_collect_points(std::vector<bool> im, std::uint64_t size)
{
    std::vector<std::pair<float, float>> points;

    for (std::uint64_t y = 0; y < size; ++y) {
        float ci = std::lerp(-2.0f, 2.0f, static_cast<float>(y) / size);
        for (std::uint64_t x = 0; x < size; ++x) {
            float cr = std::lerp(-2.0f, 2.0f, static_cast<float>(x) / size);
            if (im[y * size + x]) {
                points.emplace_back(cr, ci);
            }
        }
    }

    return points;
}

template <typename T>
struct BuddhabrotThread {
    std::uint64_t size;
    std::uint64_t max_iter;
    std::vector<std::uint64_t> counts;
    float p_uniform;
    const std::vector<std::pair<float, float>>& good_points;
    float point_radius;
    std::uint64_t progress;

    void sample(std::uint64_t n_points)
    {
        std::random_device rd;
        std::default_random_engine eng(rd());
        std::uniform_real_distribution uniform(T{-2.0}, T{2.0});
        std::bernoulli_distribution use_uniform(p_uniform);
        std::uniform_int_distribution point_idx_dist(0UL, good_points.size() - 1);

        std::vector<std::complex<T>> points;
        points.reserve(max_iter);

        for (std::uint64_t k = 0; k < n_points; ++k) {
            if (k % 1000 == 0) {
                progress = k + 1;
            }

            std::complex<T> c;
            if (use_uniform(eng)) {
                c = std::complex<T> { uniform(eng), uniform(eng) };
            } else {
                auto [rmid, imid] = good_points[point_idx_dist(eng)];
                std::uniform_real_distribution<T> r_dist(rmid - point_radius, rmid + point_radius);
                std::uniform_real_distribution<T> i_dist(imid - point_radius, imid + point_radius);

                c = std::complex<T> { r_dist(eng), i_dist(eng) };
            }

            points.clear();
            std::complex<T> z {};

            for (std::uint64_t i = 0; i < max_iter && std::norm(z) < T{8.0}; ++i) {
                z = z * z + c;
                points.push_back(z);
            }

            if (std::norm(z) < T{4.0}) {
                continue;
            }

            for (auto z_sample : points) {
                if (std::abs(z_sample.real()) > T{2.0} || std::abs(z_sample.imag()) > T{2.0}) {
                    continue;
                }

                std::int64_t x = remap<T>(-2.0, 2.0, 0, size - 1, z_sample.real());
                std::int64_t y = remap<T>(-2.0, 2.0, 0, size - 1, z_sample.imag());

                counts[y * size + x]++;
                counts[(size - y - 1) * size + x]++;
            }
        }

        progress = n_points;
    }
};

void print_duration(std::ostream& os, float secs)
{
    int whole_secs = std::lround(secs);
    int mins = whole_secs / 60;
    int mod_secs = whole_secs % 60;

    os << std::setfill('0') << std::setw(2) << mins << ":";
    os << std::setfill('0') << std::setw(2) << mod_secs;
}

void print_progress(std::uint64_t progress, std::uint64_t max_progress, std::chrono::duration<float> elapsed)
{
    std::uint64_t width = 32;

    std::string csi = "\033[";
    std::cout << csi << "1K" << csi << "G";

    std::vector<std::string> eighths {
        " ",
        "▏",
        "▎",
        "▍",
        "▌",
        "▋",
        "▊",
        "▊",
        "█"
    };

    int whole = (width * progress) / max_progress;
    int part = (8 * width * progress) / max_progress - 8 * whole;

    std::cout << "[";
    for (int i = 0; i < whole; ++i) {
        std::cout << eighths.back();
    }

    if (progress != max_progress) {
        std::cout << eighths[part] << std::string(width - whole - 1, ' ');
    }

    float ratio = static_cast<float>(progress) / max_progress;
    std::cout << "] " << std::fixed << std::setprecision(1) << (100.0f * ratio) << "%";

    std::cout << "(";
    float estimate = elapsed.count() / ratio;
    print_duration(std::cout, elapsed.count());
    std::cout << "/";
    if (elapsed.count() < 2.0f) {
        std::cout << "--:--";
    } else {
        print_duration(std::cout, estimate);
    }
    std::cout << ")";

    std::cout.flush();
}

template <typename T>
std::vector<std::uint64_t> merge_results(const std::vector<BuddhabrotThread<T>>& threads)
{
    std::vector<std::uint64_t> result(threads.front().size * threads.front().size);

    for (const auto& thread : threads) {
        std::transform(result.begin(), result.end(), thread.counts.begin(), result.begin(), [](std::uint64_t cur, std::uint64_t res) { return cur + res; });
    }

    return result;
}

std::vector<std::pair<float, float>> find_good_points(std::uint64_t size, std::uint64_t max_iter, std::uint64_t n_dilations)
{
    std::cout << "Rendering binary mandelbrot ... ";
    std::cout.flush();
    std::vector<bool> im = binary_mandelbrot(size, max_iter);
    std::cout << "done\n";

    std::cout << "Collecting edge points ... ";
    std::cout.flush();
    std::vector<bool> result = im_edge(im, size);
    std::vector<bool> reverse_edge = im_edge(im_invert(im), size);
    result = im_or(result, reverse_edge);

    for (std::uint64_t i = 0; i < n_dilations; ++i) {
        im = im_dilate(im, size);
        std::vector<bool> dilated_reverse_edge = im_edge(im_invert(im), size);
        result = im_or(result, dilated_reverse_edge);
    }

    auto good_points = im_collect_points(result, size);
    std::cout << "done\n";

    return good_points;
}

int main()
{
    std::int64_t size = 4096;

    auto good_points = find_good_points(size, 1000, 2);

    std::size_t n_threads = 12;
    std::vector<std::thread> threads(n_threads);

    BuddhabrotThread<double> buddha_template {
        static_cast<uint64_t>(size),
        20,
        std::vector<std::uint64_t>(size * size),
        1.0,
        good_points,
        2.0f / size,
        0
    };

    std::uint64_t points_per_thread = 100000000;

    std::cout << "Sampling Buddhabrot data...\n";
    std::vector buddha_threads(n_threads, buddha_template);
    for (std::size_t i = 0; i < n_threads; ++i) {
        threads[i] = std::thread(&BuddhabrotThread<double>::sample, &buddha_threads[i], points_per_thread);
    }

    bool done = false;
    auto start = std::chrono::steady_clock::now();
    while (!done) {
        using namespace std::chrono_literals;

        std::uint64_t total_progress = 0;
        for (std::size_t i = 0; i < n_threads; ++i) {
            total_progress += buddha_threads[i].progress;
        }

        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> elapsed = now - start;
        print_progress(total_progress, points_per_thread * n_threads, elapsed);
        std::this_thread::sleep_for(100ms);

        done = (total_progress >= points_per_thread * n_threads);
    }

    for (std::size_t i = 0; i < n_threads; ++i) {
        threads[i].join();
    }

    std::cout << "\nMerging thread results ... ";
    std::cout.flush();
    std::vector<std::uint64_t> result = merge_results(buddha_threads);
    std::cout << "done\n";

    std::cout << "Writing image ... ";
    std::cout.flush();

    std::vector<float> log_image;
    log_image.reserve(result.size());

    std::transform(result.begin(), result.end(), std::back_inserter(log_image), [](std::uint64_t v) { return std::log(std::max(1.0f, (float)v)); });
    auto [vmin, vmax] = std::minmax_element(log_image.begin(), log_image.end());

    auto to_int_rgb = [](float v) { return std::clamp(0, 255, static_cast<int>(256 * v)); };
    auto cmap = Colormap<float>::by_name("mako");
    cmap.set_vrange(*vmin, *vmax);
    //cmap.set_vrange(2.0, *vmax);

    std::ofstream ofs("out20.ppm");
    ofs << "P3\n";
    ofs << size << " " << size << "\n";
    ofs << "255\n";

    for (std::size_t y = 0; y < size; ++y) {
        for (std::size_t x = 0; x < size; ++x) {
            auto color = cmap(log_image[y * size + x]);
            std::transform(color.begin(), color.end(), std::ostream_iterator<int>(ofs, " "), to_int_rgb);
        }
    }

    std::cout << "done\n";
}
