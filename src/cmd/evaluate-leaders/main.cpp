#include <iostream>

#include <boost/program_options.hpp>

#include "mp/following_detection.hpp"

#include "../common/eval_leader_file.hpp"
#include "../common/follower_file.hpp"
#include "../common/leader_file.hpp"
#include "../common/ground_truth_file.hpp"
#include "../common/parser.hpp"
#include "../common/util.hpp"

using namespace std;
using namespace mp;

binary_classifier_result eval_leaders(const string &name,
                                      const leader_data &fd,
                                      const ground_truth &gt);
void parse_options(int argc, char *argv[]);
void validate_options();

string in_file;
string out_file;

string gt_file;

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "C");
    parse_options(argc, argv);
    validate_options();

    feature_parameters params;
    ground_truth gt;
    leader_data ld;

    {
        fstream in_stream;
        try {
            try_open(in_stream, in_file, ios_base::in);
        } catch (const std::exception &e) {
            cerr << "failed to open input file \""
                 << in_file << "\": "
                 << e.what() << endl;
            exit(1);
        }

        cereal::JSONInputArchive ar(in_stream);
        load_leader_file(ar, ld, params);
    }

    {
        fstream gt_stream;
        try {
            try_open(gt_stream, gt_file, ios_base::in);
        } catch (const std::exception &e) {
            cerr << "failed to open ground truth file \""
                 << gt_file << "\": "
                 << e.what() << endl;
            exit(1);
        }

        cereal::JSONInputArchive ar(gt_stream);
        load_ground_truth_file(ar, gt);
    }

    cout << "Evaluating leader file:\n"
         << "  Source:       " << in_file << "\n"
         << "  Ground truth: " << gt_file << "\n"
         << "  Data source:  " << params.data_source << "\n"
         << "  Algorithm:    " << params.algorithm << "\n"
         << "  Window size:  " << params.window_size << " seconds\n"
         << "  Time lag:     " << params.time_lag << " seconds\n"
         << "  Output file:  " << out_file << "\n"
         << flush;

    binary_classifier_result result;
    double seconds = execution_seconds([&]{
        result = eval_leaders(in_file, ld, gt);
    });
    cout << "Evaluation took " << seconds << " seconds." << endl;

    {
        fstream out_stream;
        try {
            try_open(out_stream, out_file, ios_base::out);
        } catch (const std::exception &e) {
            cerr << "failed to open output file \""
                 << out_file << "\": "
                 << e.what() << endl;
            exit(1);
        }

        cereal::JSONOutputArchive ar(out_stream);
        save_eval_leader_file(ar, params, result);
    }

    return 0;
}

// A group may have more than one leader.
unordered_map<i32, vector<string>> gt_leaders_at(const ground_truth &gt, i64 ts)
{
    using device = ground_truth::device;

    unordered_map<i32, vector<string>> leaders;

    auto it = gt.timestamps.find(ts);
    if (it == gt.timestamps.end()) {
        cerr << "Timestamp not in ground truth: " << ts << endl;
        exit(1);
    }

    const vector<device> &devs = it->second;
    for (const device &dev : devs) {
        // Only the leader of a group may have order 0
        if (dev.order == 0) {
            leaders[dev.group].push_back(dev.name);
        }
    }

    return leaders;
}

binary_classifier_result eval_leaders(const string &name,
                                      const leader_data &ld,
                                      const ground_truth &gt)
{
    const i64 num_devices = static_cast<i64>(ld.devices.size());

    i64 false_positive = 0;
    i64 false_negative = 0;
    i64 positives = 0;
    i64 negatives = 0;

    for (i64 ts = ld.begin_timestamp; ts <= ld.end_timestamp; ++ts) {
        auto gt_leaders = gt_leaders_at(gt, ts);

        // Returns true iff "l" is a leader in any of the gt-groups.
        auto is_gt_leader = [&](const string &l) {
            for (auto &pair : gt_leaders) {
                const vector<string> &group = pair.second;
                if (std::find(group.begin(), group.end(), l) != group.end()) {
                    return true;
                }
            }
            return false;
        };

        const vector<string> &leaders = ld.data_at(ts).leaders;

        const i64 pos = static_cast<i64>(gt_leaders.size());
        positives += pos;
        negatives += (num_devices - pos);

        for (auto &pair : gt_leaders) {
            const vector<string> &group = pair.second;
            // It is enough if one of the real leaders was detected.
            bool detected = std::any_of(group.begin(), group.end(),
                                        [&](const string &gt_leader) {
                return std::find(leaders.begin(), leaders.end(), gt_leader) != leaders.end();
            });

            if (!detected) {
                // A leader of a group was not classified as such.
                ++false_negative;
            }
        }

        for (auto &l : leaders) {
            if (!is_gt_leader(l)) {
                // A follower was classified as a leader.
                ++false_positive;
            }
        }
    }

    return binary_classifier_result(
                name,
                // True positive, false positive
                positives - false_negative, false_positive,
                // False negative, true negative
                false_negative, negatives - false_positive);
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
             "The path to a file generated by the detect-leaders program.")
            ("ground-truth",
             po::value<string>(&gt_file)->value_name("PATH")->required(),
             "The ground truth. Leader results will be compared against this data.")
            ("output",
             po::value<string>(&out_file)->value_name("PATH")->required(),
             "The output path. The evaluation results will be written to this file.")
            ;

    po::variables_map vm;
    try {
        auto parser = po::command_line_parser(argc, argv);
        parser.options(opts);
        po::store(parser.run(), vm);

        if (vm.count("help")) {
            cerr << "Usage: " << argv[0] << " [options]" << "\n";
            cerr << "\n";
            cerr << "This program reads a leader file created by the detect-leaders program\n"
                 << "and evaluates its accuracy.\n\n"
                 << flush;
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
    bool ok = true;

    if (!ok) {
        exit(1);
    }
}
