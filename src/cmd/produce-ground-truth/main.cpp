#include <algorithm>
#include <fstream>
#include <iostream>

#include <boost/program_options.hpp>
#include <cereal/archives/json.hpp>

#include "mp/ground_truth.hpp"

#include "../common/ground_truth_file.hpp"
#include "../common/parser.hpp"
#include "../common/scene_manifest.hpp"
#include "../common/util.hpp"

using namespace std;
using namespace mp;

void parse_options(int argc, char *argv[]);
void validate_options();

string in_file;
string out_file;

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "C");
    parse_options(argc, argv);
    validate_options();

    ground_truth gt;
    scene_manifest sm = read_scene_manifest(in_file);
    if (sm.scene_type == "plain") {
        gt = read_ground_truth_file(
                    sm.get_plain_scene_data().ground_truth_file);
    } else if (sm.scene_type == "game") {
        gt = read_game_ground_truth(sm);
    } else {
        assert(false);
    }

    {
        fstream out_stream;
        try {
            try_open(out_stream, out_file, ios_base::out);
        } catch (const std::exception &e) {
            cerr << "failed to open output file file \""
                 << out_file << "\": "
                 << e.what() << endl;
            exit(1);
        }

        cereal::JSONOutputArchive ar(out_stream);
        save_ground_truth_file(ar, gt);
    }
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
              "The input file must be a scene manifest.")
            ("output",
             po::value<string>(&out_file)->value_name("PATH")->required(),
             "Output file.")
            ;

    po::variables_map vm;
    try {
        auto parser = po::command_line_parser(argc, argv);
        parser.options(opts);
        po::store(parser.run(), vm);

        if (vm.count("help")) {
            cerr << "Usage: " << argv[0] << " [options]" << "\n";
            cerr << "\n";
            cerr << "This program reads a scene manifest and produces a ground truth file.\n"
                 << "Ground truth files are consumed by other programs to evaluate predictions.\n\n";
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
