#include "parser.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>

#include <boost/filesystem.hpp>

#include "mp/parser.hpp"
#include "mp/signal_data.hpp"

#include "util.hpp"

namespace fs = boost::filesystem;

using namespace mp;
using namespace std;

namespace {

void average_access_points(const tracing_data &td)
{
    cout << "Calculating average number of access points "
         << "across all timestamps and devices...\n"
         << flush;

    i64 total = 0;
    i64 count = 0;
    for (auto &dev : td.devices) {
        for (i64 ts = td.min_timestamp; ts <= td.max_timestamp; ++ts) {
            auto has_data = td.has_data_at(dev, ts);
            total += std::count(has_data.begin(), has_data.end(), 1);
            ++count;
        }
    }

    cout << "  Average: " << (double(total) / count) << "\n"
         << endl;
}

void clean_access_points(signal_data &sd, double minimum_average)
{
    vector<i32> bad_aps = bad_access_points(sd, minimum_average);
    if (bad_aps.empty()) {
        return;
    }

    cout << "Removing " << bad_aps.size() << " access points from input data.\n"
         << "Their average signal strenth is below " << minimum_average << " dBm:\n";
    for (i32 ap : bad_aps) {
        cout << "  " << sd.bssids[ap] << "\n";
    }
    cout << endl;

    remove_access_points(sd, bad_aps);
    if (sd.bssids.empty()) {
        cout << "There are no access points left. Exiting." << endl;
        exit(0);
    }
}

} // namespace

tracing_data read_signal_file(const string &path,
                              double minimum_average,
                              i32 missing_reading)

{
    tracing_data trace;
    {
        fstream signal_stream;
        try {
            try_open(signal_stream, path, ios_base::in);
        } catch (const std::exception &e) {
            cerr << "failed to open input file \""
                 << path << "\": "
                 << e.what() << endl;
            exit(1);
        }

        auto signal = parse_signal_data(signal_stream);
        clean_access_points(signal, minimum_average);
        trace = transform(signal, missing_reading);
        average_access_points(trace);
    }
    return trace;
}

tracing_data read_location_file(const string &path)
{
    tracing_data trace;
    {
        fstream location_stream;
        try {
            try_open(location_stream, path, ios_base::in);
        } catch (const std::exception &e) {
            cerr << "failed to open input file \""
                 << path << "\": "
                 << e.what() << endl;
            exit(1);
        }

        auto loc = parse_location_data(location_stream);
        trace = transform(loc);
    }
    return trace;
}

ground_truth read_ground_truth_file(const string &path)
{
    ground_truth gt;
    {
        fstream gt_stream;
        try {
            try_open(gt_stream, path, ios_base::in);
        } catch (const std::exception &e) {
            cerr << "failed to open ground truth file \""
                 << path << "\": "
                 << e.what() << endl;
            exit(1);
        }
        gt = parse_ground_truth_data(gt_stream);
    }
    return gt;
}

scene_manifest read_scene_manifest(const string &path)
{
    scene_manifest sm;
    {
        fstream f;
        try {
            try_open(f, path, ios_base::in);
            sm.load(f);
            if (!sm.validate(cout)) {
                cerr << "invalid game manifest" << endl;
                exit(1);
            }
        } catch (const std::exception &e) {
            cerr << "failed to read manifest file \""
                 << path << "\": "
                 << e.what() << endl;
            exit(1);
        }
    }

    // Fix relative paths.
    // They are relative to the location of the manifest file.
    fs::path manifest_folder = fs::path(path).parent_path();
    auto relative_path = [&](string &path) {
        fs::path p(path);
        if (p.is_relative()) {
            p = manifest_folder / p;
        }
        path = p.string();
    };

    if (sm.scene_type == "plain") {
        plain_scene_data &p = sm.get_plain_scene_data();
        relative_path(p.data_file);
        if (p.ground_truth_file != "") {
            relative_path(p.ground_truth_file);
        }
    } else if (sm.scene_type == "game") {
        game_scene_data &g = sm.get_game_scene_data();
        relative_path(g.folder);
        if (g.location_file != "") {
            relative_path(g.location_file);
        }
    }

    return sm;
}

tracing_data read_game_signal_files(const scene_manifest &sm,
                                    double minimum_average,
                                    i32 missing_reading)
{
    const game_scene_data &gd = sm.get_game_scene_data();

    fs::path game_dir(gd.folder);
    game_signal_data_parser p;
    for (const string &target : sm.targets) {
        fs::path scan = (game_dir / target).replace_extension(".scanresult.csv");
        fstream scan_stream;
        try {
            try_open(scan_stream, scan.string(), ios_base::in);
        } catch (const std::exception &e) {
            cerr << "failed to open scanresult file \""
                 << scan.string() << "\": "
                 << e.what() << endl;
            exit(1);
        }

        p.parse(target, scan_stream);
    }

    signal_data data = p.take();
    clean_access_points(data, minimum_average);
    tracing_data td = transform(data, missing_reading);
    average_access_points(td);
    return td;
}

ground_truth read_game_ground_truth(const scene_manifest &sm)
{
    const game_scene_data &gd = sm.get_game_scene_data();

    ground_truth gt;
    {
        fs::path game_dir(gd.folder);
        game_ground_truth_parser p(gd.evaders, sm.start, sm.end);
        for (const string &target : sm.targets) {
            fs::path follow = (game_dir / target).replace_extension(".followevent.csv");
            fstream follow_stream;
            try {
                try_open(follow_stream, follow.string(), ios_base::in);
            } catch (const std::exception &e) {
                cerr << "failed to open followevent file \""
                     << follow.string() << "\": "
                     << e.what() << endl;
                exit(1);
            }

            p.parse(target, follow_stream);
        }
        gt = p.take();
    }
    return gt;
}

