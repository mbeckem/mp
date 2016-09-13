#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>

#define PICOJSON_USE_INT64
#include <picojson/picojson.h>

#include "mp/metrics.hpp"

using namespace std;
using namespace mp;

namespace pj = picojson;

template<typename T, typename U>
ostream& operator <<(ostream &o, const tuple<T, U> &t)
{
    o << "(" << get<0>(t) << ", " << get<1>(t) << ")";
    return o;
}

template<typename T>
ostream& operator <<(ostream &o, const vector<T> &vec)
{
    auto b = vec.begin();
    auto e = vec.end();
    for (auto i = b; i != e; ++i) {
        if (i != b) {
            o << " ";
        }
        o << *i;
    }
    return o;
}

template<typename T>
ostream& operator <<(ostream &o, const array_2d<T> &mat)
{
    for (size_t i = 0; i < mat.rows(); ++i) {
        for (size_t j = 0; j < mat.columns(); ++j) {
            if (j != 0) {
                o << " ";
            }
            o << mat.cell(i, j);
        }
        o << "\n";
    }
    return o;
}

template<typename T>
string to_string(const T &t)
{
    std::ostringstream o;
    o << t;
    return o.str();
}

pj::value path_to_json(const vector<tuple<size_t, size_t>> &path)
{
    pj::array a;
    for (auto &t : path) {
        pj::array p;
        p.push_back(pj::value(i64(get<0>(t))));
        p.push_back(pj::value(i64(get<1>(t))));
        a.push_back(pj::value(p));
    }
    return pj::value(a);
}

int main()
{
    vector<double> series_a;
    vector<double> series_b;

    for (int i = 0; i < 100; ++i) {
        double a = sin(double(i) / 15);
        double b = sin(double(i) / 15 * 0.9) + 0.4;

        series_a.push_back(a);
        series_b.push_back(b);
    }

    dtw d(series_a.size(), series_b.size());
    double cost = d.run(series_a, series_b, manhattan_distance_1);
    auto cost_matrix = d.cost_matrix();
    auto warp_path = d.warp_path();

    pj::object items{
        {"series_a", pj::value(to_string(series_a))},
        {"series_b", pj::value(to_string(series_b))},
        {"cost", pj::value(cost)},
        {"warp_path", path_to_json(warp_path)},
        {"cost_matrix", pj::value(to_string(cost_matrix))},
    };
    cout << pj::value(items).serialize(true) << endl;
}
