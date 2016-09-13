#include <iostream>

#include <boost/program_options.hpp>
#include <cereal/archives/json.hpp>

#include "mp/following_graph.hpp"

#include "../common/util.hpp"
#include "../common/follower_file.hpp"
#include "../common/ground_truth_file.hpp"
#include "../common/leader_file.hpp"

using namespace mp;
using namespace std;

void parse_options(int argc, char *argv[]);
void validate_options();

string in_file;
string out_file;
bool use_weights = false;

string gt_file;

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "C");
    parse_options(argc, argv);
    validate_options();

    following_data fd;
    feature_parameters params;

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

    cout << "Detecting leaders:\n"
         << "  Source: " << in_file << "\n"
         << "  Data source: " << params.data_source << "\n"
         << "  Algorithm: " << params.algorithm << "\n"
         << "  Time lag: " << params.time_lag << " seconds\n"
         << "  Window size: " << params.window_size << " seconds\n"
         << "  Edge weights: " << std::boolalpha << use_weights << "\n"
         << flush;

    leader_data ld;
    double seconds = execution_seconds([&]{
        ld = detect_leaders(fd, use_weights);
    });
    cout << "Detection took " << seconds << " seconds" << endl;

    {
        fstream out_stream;
        try {
            try_open(out_stream, out_file, ios_base::out);
        } catch (const std::exception &e) {
            cerr << "failed to open output file \""
                 << in_file << "\": "
                 << e.what() << endl;
            exit(1);
        }

        cereal::JSONOutputArchive ar(out_stream);
        save_leader_file(ar, ld, params);
    }
    return 0;
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
            ("output",
             po::value<string>(&out_file)->value_name("PATH")->required(),
             "The output path. The results will be written to this file.")
            ("use-weights",
             po::value<bool>(&use_weights)->default_value(true),
             "Whether to use edge weights in PageRank or not.")
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
                 << "and detects leaders.\n"
                 << "For every timestamp, a list of devices that have been classified as\n"
                 << "leaders of their groups will be produced.\n"
                 << "The result data will be written to the output file.\n\n"
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

}
