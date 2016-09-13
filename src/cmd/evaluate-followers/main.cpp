#include <iostream>

#include <boost/program_options.hpp>

#include "mp/following_detection.hpp"

#include "../common/eval_follower_file.hpp"
#include "../common/follower_file.hpp"
#include "../common/ground_truth_file.hpp"
#include "../common/parser.hpp"
#include "../common/util.hpp"

using namespace std;
using namespace mp;

follower_evaluation_result eval_followers(const string &name,
                                          const following_data &fd,
                                          const ground_truth &gt);
void parse_options(int argc, char *argv[]);
void validate_options();

string in_file;
string out_file;

string gt_file;

bool verbose = false;

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "C");
    parse_options(argc, argv);
    validate_options();

    feature_parameters params;
    ground_truth gt;
    following_data fd;

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
        load_follower_file(ar, fd, params);
    }

    {
        fstream gt_stream;
        try {
            try_open(gt_stream, gt_file, ios_base::in);
        } catch (const std::exception &e) {
            cerr << "failed to open ground truth file \""
                 << in_file << "\": "
                 << e.what() << endl;
            exit(1);
        }

        cereal::JSONInputArchive ar(gt_stream);
        load_ground_truth_file(ar, gt);
    }

    cout << "Evaluating follower file:\n"
         << "  Source:       " << in_file << "\n"
         << "  Ground truth: " << gt_file << "\n"
         << "  Data source:  " << params.data_source << "\n"
         << "  Algorithm:    " << params.algorithm << "\n"
         << "  Window size:  " << params.window_size << " seconds\n"
         << "  Time lag:     " << params.time_lag << " seconds\n"
         << "  Output file:  " << out_file << "\n"
         << flush;

    follower_evaluation_result result;
    double seconds = execution_seconds([&]{
        result = eval_followers(in_file, fd, gt);
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
        save_eval_follower_file(ar, params, result);
    }

    return 0;
}

follower_evaluation_result eval_followers(const string &name,
                                          const following_data &fd,
                                          const ground_truth &gt)
{
    i64 correct = 0;
    i64 total = 0;
    i64 total_co_moving = 0;

    for (i64 ts = fd.begin_timestamp; ts <= fd.end_timestamp; ++ts) {
        auto &data = fd.data_at(ts);

        i64 ts_correct = 0;
        i64 ts_total = 0;
        i64 ts_total_co_moving = 0;

        for (auto &pair : data.co_moving) {
            const string &left_name = fd.devices[pair.left];
            const string &right_name = fd.devices[pair.right];

            following_type predicted = pair.type;
            ground_truth_relation real = gt.relation_at(ts, left_name, right_name);

            if (real != ground_truth_relation::none) {
                ++ts_total_co_moving;
            }
            switch (predicted) {
            case following_type::co_leading:
                if (real == ground_truth_relation::leading) {
                    ++ts_correct;
                }
                break;
            case following_type::following:
                if (real == ground_truth_relation::following) {
                    ++ts_correct;
                }
                break;
            case following_type::leading:
                if (real == ground_truth_relation::leading) {
                    ++ts_correct;
                }
                break;
            }
            ++ts_total;
        }

        correct += ts_correct;
        total += ts_total;
        total_co_moving += ts_total_co_moving;

        double ts_acc = ts_correct == 0 ? 0 : double(ts_correct) / ts_total;
        if (verbose && ts_acc > 0.8) {
            cout << "Timestamp " << ts << " is good with accuracy " << ts_acc
                 << " (" << ts_total << " total)"
                 << endl;
        }
    }

    follower_evaluation_result result;
    result.name = name;
    result.correct = correct;
    result.total = total;
    result.total_co_moving = total_co_moving;
    result.accuracy = total == 0 ? 0 : double(correct) / total;
    result.accuracy_co_moving = total_co_moving == 0 ? 0 : double(correct) / total_co_moving;
    return result;
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
             "The path to a file generated by the detect-followers program.")
            ("ground-truth",
             po::value<string>(&gt_file)->value_name("PATH")->required(),
             "The ground truth. Follower results will be compared against this data.")
            ("output",
             po::value<string>(&out_file)->value_name("PATH")->required(),
             "The output path. The evaluation results will be written to this file.")
            ("verbose",
             po::bool_switch(&verbose),
             "Print extra messages (e.g. highlighting good timestamps).")
            ;

    po::variables_map vm;
    try {
        auto parser = po::command_line_parser(argc, argv);
        parser.options(opts);
        po::store(parser.run(), vm);

        if (vm.count("help")) {
            cerr << "Usage: " << argv[0] << " [options]" << "\n";
            cerr << "\n";
            cerr << "This program reads a follower file created by the detect-followers program\n"
                 << "and evaluates its precision.\n\n"
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
