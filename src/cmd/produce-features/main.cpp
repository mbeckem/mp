#include <iostream>
#include <iomanip>
#include <thread>

#include <boost/program_options.hpp>

#include "mp/feature_computation.hpp"
#include "mp/metrics.hpp"
#include "mp/tracing_data.hpp"
#include "mp/parser.hpp"

#include "../common/feature_file.hpp"
#include "../common/parser.hpp"
#include "../common/util.hpp"

using namespace std;
using namespace mp;

void compute_features(const tracing_data &trace,
                      const scene_manifest &sm,
                      const vector<tuple<i32, i32>> &pairs);

void clean_devices(tracing_data &trace, const vector<string> &targets);

array_2d<double> evaluate_warp_path(const tracing_data &td,
                                    const vector<tuple<i32, i32>> &pairs,
                                    const feature_computation &f);

void write_dtw_frequencies(const array_2d<double> &freqs);

void write_feature_file(const similarity_data &sim,
                        const scene_manifest &sm);

vector<tuple<i32, i32>> get_game_pairs(const tracing_data &td,
                                       const game_scene_data &g);

void parse_options(int argc, char *argv[]);

void validate_options();

const double minimum_signal_average = -90;  // dBm
const double missing_signal_reading = -100; // dBm

// The file and its type to use as input,
// e.g. a file containing signal or location measurements.
string in_file;

// The output file and the format to chose.
string out_file;
string out_type; // one of "json", "compact-json", "binary"

// Data preprocessing
int smooth;

// Algorithm and its settings
string algorithm;   // "dtw", "multi-dtw", "euclid" or "eval-dtw"
int window_size;    // > 0
int time_lag;       // >= 0
int threads;        // >= 0, 0 -> automatic

bool disable_target_filter = false;
int  limit_targets = -1;

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "C");
    parse_options(argc, argv);
    validate_options();

    scene_manifest sm = read_scene_manifest(in_file);
    tracing_data trace;
    vector<tuple<i32, i32>> pairs; // device pairs to consider

    if (sm.scene_type == "plain") {
        const plain_scene_data &p = sm.get_plain_scene_data();
        if (sm.data_type == "signal") {
            trace = read_signal_file(p.data_file, minimum_signal_average, missing_signal_reading);
        } else if (sm.data_type == "location") {
            trace = read_location_file(p.data_file);
        } else {
            assert(false);
        }

        clean_devices(trace, sm.targets);
        pairs = trace.unique_pairs();
    } else if (sm.scene_type == "game") {
        const game_scene_data &g = sm.get_game_scene_data();
        if (sm.data_type == "signal") {
            trace = read_game_signal_files(sm, minimum_signal_average, missing_signal_reading);
        } else if (sm.data_type == "location") {
            trace = read_location_file(g.location_file);
        } else {
            assert(false);
        }

        clean_devices(trace, sm.targets);
        pairs = get_game_pairs(trace, g);
    } else {
        assert(false);
    }


    if (smooth != 0) {
        cout << "Smoothing input data" << "\n"
             << "  Moving average window: " << smooth << " seconds\n"
             << flush;
        double seconds = execution_seconds([&]{
            moving_average(trace, smooth);
        });
        cout << "Smoothing took " << seconds << " seconds" << "\n" << endl;
    }

    compute_features(trace, sm, pairs);
    return 0;
}

// Compute the feature values and write them to the output file.
void compute_features(const tracing_data &trace,
                      const scene_manifest &sm,
                      const vector<tuple<i32, i32>> &pairs)
{
    // The feature data's time range must be a subset of the
    // measurement data's time range.
    if (sm.start < trace.min_timestamp) {
        ostringstream msg;
        msg << "manifest starts at timestamp " << sm.start
            << " but we have no measurements until " << trace.min_timestamp << endl;
        throw runtime_error(msg.str());
    }
    if (sm.end > trace.max_timestamp) {
        ostringstream msg;
        msg << "manifest ends at timestamp " << sm.end
            << " but measurements already stop at " << trace.max_timestamp << endl;
        throw runtime_error(msg.str());
    }

    feature_computation f;
    f.time_lag = time_lag;
    f.window_size = window_size;
    f.threads = threads ? threads
                        : max(1u, thread::hardware_concurrency());
    f.begin_timestamp = sm.start;
    f.end_timestamp = sm.end;

    cout << "Computing feature vectors:\n"
         << "  Input:          " << in_file << "\n"
         << "  Output:         " << out_file << "\n"
         << flush;
    cout << "Data attributes:\n"
         << "  Data source:    " << sm.data_type << "\n"
         << "  Begin:          " << f.begin_timestamp << "\n"
         << "  End:            " << f.end_timestamp << "\n"
         << "  Duration:       " << (f.end_timestamp - f.begin_timestamp + 1) << " seconds \n"
         << "  Devices:        " << trace.devices.size() << "\n"
         << "  Pairs:          " << pairs.size() << "\n"
         << "  Data Dimension: " << trace.data_dimension << "\n"
         << flush;
    cout << "Computation parameters:\n"
         << "  Time lag:       " << f.time_lag << " seconds\n"
         << "  Window size:    " << f.window_size << " seconds\n"
         << "  Threads:        " << f.threads << "\n"
         << "  Algorithm:      " << algorithm << "\n"
         << flush;

    if (algorithm == "eval-dtw") {
        cout << "Running dtw evaluation" << endl;

        array_2d<double> freqs;
        double seconds = execution_seconds([&]{
            freqs = evaluate_warp_path(trace, pairs, f);
        });

        cout << "Evaluation took " << seconds << " seconds" << endl;
        write_dtw_frequencies(freqs);
    } else {
        cout << "Computing feature values" << endl;

        similarity_data result;
        double seconds = execution_seconds([&]{
            if (algorithm == "dtw") {
                result = f.compute_dtw(trace, pairs);
            } else if (algorithm == "multi-dtw") {
                result = f.compute_multi_dtw(trace, pairs);
            } else if (algorithm == "euclid") {
                result = f.compute_euclid(trace, pairs);
            } else {
                throw logic_error("unsupported algorithm");
            }
        });
        cout << "Computation took " << seconds << " seconds" << endl;

        write_feature_file(result, sm);
    }
}

// Removes all devices from "trace" that are never mentioned in "gt".
void clean_devices(tracing_data &trace, const vector<string> &targets)
{
    if (!disable_target_filter) {
        vector<string> rm;
        for (auto &dev : trace.devices) {
            if (std::find(targets.begin(), targets.end(), dev.name) == targets.end()) {
                rm.push_back(dev.name);
            }
        }

        if (!rm.empty()) {
            cout << "Removing " << rm.size() << " devices from input data.\n"
                 << "They are not mentioned in the list of targets:\n";
            for (auto &name : rm) {
                cout << "  " << name << "\n";
            }
            cout << endl;

            auto predicate = [&](const tracing_data::device_data &dev) {
                return std::find(rm.begin(), rm.end(), dev.name) != rm.end();
            };
            auto iter = std::remove_if(trace.devices.begin(), trace.devices.end(), predicate);
            trace.devices.erase(iter, trace.devices.end());
        }
    } else {
        cout << "Not filtering devices based on list of targets" << endl;
    }

    if (limit_targets >= 0) {
        if (trace.devices.size() > static_cast<size_t>(limit_targets)) {
            trace.devices.resize(limit_targets);
        }
        cout << "Number of devices has been limited to " << limit_targets << endl;
    }
}

/*
 * This is a quick and dirty reimplementation of the code
 * found in feature_computation.cpp.
 * Instead of computing any real feature vectors,
 * this struct runs the dtw computation of all values for the solo purpose
 * of getting the warp path for every such value.
 *
 * Internally, the warp path indices are stored and counted.
 * Use index_frequencies() to create a table that contains the relative frequencies
 * of warp path indicies.
 * E.g. if index_frequencies().cell(i, j) is 0.5,
 * then every second warp path passed through that index.
 */
struct dtw_path
{
    dtw_path(const tracing_data &td, i32 window_size)
        : td(td)
        , begin(td.min_timestamp)
        , end(td.max_timestamp)
        , input_dimension(td.data_dimension)
        , window_size(window_size)
        , d(window_size, window_size)
        , counters(window_size, window_size, 0)
        , left_buf(window_size)
        , right_buf(window_size)
    {
        // Worst case path length:
        path_buf.reserve(2 * window_size);
    }

    void compute_similarity(i64 ts, i32 lag,
                            const tracing_data::device_data &left,
                            const tracing_data::device_data &right)
    {
        auto ts_bounds = [&](i64 i) {
            if (i < begin) {
                return begin;
            } else if (i > end) {
                return end;
            } else {
                return i;
            }
        };
        auto ts_range = [&](i64 i) {
            assert(end - begin + 1 >= window_size);
            if (i < begin) {
                return begin;
            } else if (i + window_size - 1 > end) {
                return end - window_size + 1;
            } else {
                return i;
            }
        };

        auto left_has_data = td.has_data_at(left, ts);
        auto right_has_data = td.has_data_at(right, ts_bounds(ts + lag));

        i64 left_ts = ts - (window_size / 2);
        i64 right_ts = left_ts + lag;
        left_ts = ts_range(left_ts);
        right_ts = ts_range(right_ts);

        for (i32 col = 0; col < input_dimension; ++col) {
            if (left_has_data[col] || right_has_data[col]) {
                for (i32 j = 0; j < window_size; ++j) {
                    left_buf[j] = td.data_at(left, left_ts + j)[col];
                    right_buf[j] = td.data_at(right, right_ts + j)[col];
                }
                compute_dtw(left_buf, right_buf);
            }
        }
    }

    void compute_dtw(array_view<const double> a,
                     array_view<const double> b)
    {
        d.run(a, b, manhattan_distance_1);
        d.warp_path(path_buf);
        for (const auto &e : path_buf) {
            size_t i = get<0>(e);
            size_t j = get<1>(e);
            ++counters.cell(i, j);
        }
        ++counter;
    }

    array_2d<double> index_frequencies() const
    {
        if (counter == 0) {
            throw std::logic_error("must call compute_dtw at least once.");
        }

        array_2d<double> result(window_size, window_size);
        for (i32 i = 0; i < window_size; ++i) {
            for (i32 j = 0; j < window_size; ++j) {
                result.cell(i, j) = double(counters.cell(i, j)) / counter;
            }
        }
        return result;
    }

    const tracing_data &td;
    const i64 begin;
    const i64 end;
    const i32 input_dimension;
    const i32 window_size;

    dtw d;
    i64 counter = 0;
    array_2d<i64> counters;
    vector<double> left_buf;
    vector<double> right_buf;
    vector<tuple<size_t, size_t>> path_buf;
};

// Returns a matrix of relative frequencies.
// A cell (i, j) contains the relative frequencies of a warp path
// passing it (1.0 -> every warp path crosses this cell).
array_2d<double> evaluate_warp_path(const tracing_data &td,
                                    const vector<tuple<i32, i32>> &pairs,
                                    const feature_computation &f)
{
    static const i32 window_size = f.window_size;
    static const i32 time_lag = f.time_lag;

    dtw_path d(td, window_size);
    for (auto &pair : pairs) {
        auto &ldev = td.devices.at(get<0>(pair));
        auto &rdev = td.devices.at(get<1>(pair));

        for (i64 ts = f.begin_timestamp; ts <= f.end_timestamp; ++ts) {
            for (i32 lag = -time_lag; lag <= time_lag; ++lag) {
                d.compute_similarity(ts, lag, ldev, rdev);
            }
        }
    }

    return d.index_frequencies();
}

// Writes the matrix of relative frequencies to the output file.
void write_dtw_frequencies(const array_2d<double> &freqs)
{
    fstream out;
    try {
        try_open(out, out_file, ios_base::out);
    } catch (const std::exception &e) {
        cerr << "failed to open output file \""
             << out_file << "\": "
             << e.what() << endl;
        exit(1);
    }

    out << fixed
        << showpoint
        << setprecision(4);
    for (size_t i = 0; i < freqs.rows(); ++i) {
        for (size_t j = 0; j < freqs.columns(); ++j) {
            if (j != 0) {
                out << " ";
            }
            out << freqs.cell(i, j);
        }
        out << "\n";
    }
    out << flush;
}

// Writes the feature values to the output file.
void write_feature_file(const similarity_data &sim, const scene_manifest &sm)
{
    // Serialize ground truth and feature vectors
    feature_parameters params;
    params.data_source = sm.data_type;
    params.algorithm = algorithm;
    params.window_size = window_size;
    params.time_lag = time_lag;

    try {
        write_feature_file(out_file, out_type, sim, params);
    } catch (const std::exception &e) {
        cerr << "failed to write feature file \""
             << out_file << "\": "
             << e.what() << endl;
        exit(1);
    }
}

// Due to the nature of the game experiment data,
// only those pairs where one device is an evader
// and one is a follower can be (realiably) computed.
vector<tuple<i32, i32>> get_game_pairs(const tracing_data &td, const game_scene_data &g)
{
    vector<tuple<i32, i32>> pairs;
    const i32 n = numeric_cast<i32>(td.devices.size());
    for (i32 i = 0; i < n; ++i) {
        const string &ldev = td.devices[i].name;
        if (!g.is_evader(ldev)) {
            continue;
        }

        for (i32 j = 0; j < n; ++j) {
            const string &rdev = td.devices[j].name;
            if (g.is_evader(rdev)) {
                continue;
            }

            pairs.push_back(make_tuple(i, j));
        }
    }
    return pairs;
}

void parse_options(int argc, char *argv[])
{
    namespace po = boost::program_options;

    po::options_description opts("Options");
    opts.add_options()
            ("help,h",
             "Print this help message")
            ("input",
             po::value<string>(&in_file)->value_name("PATH")->required(),
             "The input file must be a valid scene manifest. The manifest describes the parameters of the experiment.")
            ("output",
             po::value<string>(&out_file)->value_name("PATH")->required(),
             "Output file")
            ("output-type",
             po::value<string>(&out_type)->value_name("TYPE")->required(),
             "The output type.\n"
             "Supported formats:\n"
             "  json:         \tA pretty-printed JSON document (usually huge but human readable).\n"
             "  compact-json: \tA JSON document without any formatting. Requires a pretty printer to be readable.\n"
             "  binary:       \tA compact binary format (small output, good performance).\n"
             "  plain:        \tA plain text file. This is only supported for the eval-dtw algorithm.")
            ("smooth",
             po::value<int>(&smooth)->value_name("SECONDS")->default_value(0),
             "The input data can be smoothed by taking the moving average for every timestamp.\n"
             "This parameter specifies the window size for the moving average in seconds.\n"
             "0 means disabled (the default).")
            ("algorithm",
             po::value<string>(&algorithm)->value_name("NAME")->default_value("dtw"),
             "The similarity algorithm which will be used to compute all features values.\n"
             "Supported algorithms are:\n"
             "  dtw:       \tUse dynamic time warp as a distance metric between time-lagged sequences.\n"
             "  multi-dtw: \tUse the multi-dimensional variant of DTW as a distance metric.\n"
             "  euclid:    \tUse the euclidean distance to compute the similarity between two time series.\n"
             "  eval-dtw:  \tThis is a pseudo algorithm that will output a table of relative frequencies for "
                            "dtw warp paths. This algorithms takes no settings (e.g. time-lag, window-size) and the only "
                            "supported output format is \"plain\".")
            ("window-size",
             po::value<int>(&window_size)->value_name("SECONDS")->default_value(15),
             "The window size must be greater than zero.")
            ("time-lag",
             po::value<int>(&time_lag)->value_name("SECONDS")->default_value(7),
             "The time lag must be equal to or greater than zero.")
            ("threads",
             po::value<int>(&threads)->value_name("NUMBER")->default_value(0),
             "The number of threads. 0 means automatic, greater values specifiy the exact number.")
            ("disable-target-filter",
             po::bool_switch(&disable_target_filter),
             "Disable filtering of devices based on the target list. Only used for performance evaluation.")
            ("limit-targets",
             po::value<int>(&limit_targets)->value_name("NUMBER")->default_value(-1),
             "Limit the number of devices to the given number. Used for performance evaluation")
            ;

    po::variables_map vm;
    try {
        auto parser = po::command_line_parser(argc, argv);
        parser.options(opts);
        po::store(parser.run(), vm);

        if (vm.count("help")) {
            cerr << "Usage: " << argv[0] << " [options]" << "\n";
            cerr << opts << flush;
            exit(1);
        }
        po::notify(vm);
    } catch (const po::error &e) {
        cerr << e.what() << endl;
        exit(1);
    }
}

void validate_options()
{
    auto contains = [](const vector<string> &strings, const string &s) {
        return std::find(strings.begin(), strings.end(), s) != strings.end();
    };

    static const vector<string> allowed_output_types{"json", "compact-json", "binary", "plain"};
    static const vector<string> allowed_algorithms{"dtw", "multi-dtw", "euclid", "eval-dtw"};

    bool ok = true;
    if (!contains(allowed_output_types, out_type)) {
        cerr << "output type is invalid (" << out_type << ")" << endl;
        ok = false;
    }
    if (!contains(allowed_algorithms, algorithm)) {
        cerr << "algorithm is invalid (" << algorithm << ")" << endl;
        ok = false;
    }
    if (algorithm == "eval-dtw" && out_type != "plain") {
        cerr << "algorithm eval-dtw requires plain output format" << endl;
        ok = false;
    }
    if (algorithm != "eval-dtw" && out_type == "plain") {
        cerr << "unsupported output format for algorithm " << algorithm << " (" << out_type << ")" << endl;
        ok = false;
    }
    if (smooth < 0) {
        cerr << "smoothing parameter must be greater than or equal to zero (" << smooth << ")" << endl;
        ok = false;
    }
    if (window_size <= 0) {
        cerr << "window size must be greater than zero (" << window_size << ")" << endl;
        ok = false;
    }
    if (time_lag < 0) {
        cerr << "time lag must be greater than or equal to zero (" << time_lag << ")" << endl;
        ok = false;
    }
    if (threads < 0) {
        cerr << "threads must be greater than or equal to zero (" << threads << ")" << endl;
        ok = false;
    }

    if (!ok) {
        exit(1);
    }
}
