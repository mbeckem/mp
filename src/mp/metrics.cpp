#include "mp/metrics.hpp"

#include <algorithm>

namespace mp {

void dtw::warp_path(vector<tuple<size_t, size_t>> &v) const
{
    v.clear();

    auto at = [&](size_t row, size_t col) {
        return buffer.cell(row, col);
    };
    auto min = [](double a, double b, double c) {
        return std::min(a, std::min(b, c));
    };

    size_t i = buffer.rows() - 1;      // row index
    size_t j = buffer.columns() - 1;   // column index
    v.push_back(make_tuple(i, j));

    while (i > 0 || j > 0) {
        if (i == 0) {
            --j;
        } else if (j == 0) {
            --i;
        } else {
            double cost = min(at(i - 1, j),
                              at(i, j - 1),
                              at(i - 1, j - 1));
            // Check first to prefer diagonal path
            if (cost == at(i - 1, j - 1)) {
                --i;
                --j;
            } else if (cost == at(i, j - 1)) {
                --j;
            } else {
                --i; // cost == at(i - 1, j)
            }
        }

        v.push_back(make_tuple(i, j));
    }

    std::reverse(v.begin(), v.end());
}

} // namespace mp
