
#include <iostream>
#include <fstream>

#include <boost/program_options.hpp>

#include "mp/following_detection.hpp"
#include "mp/following_graph.hpp"
#include "mp/tracing_data.hpp"

#include "../common/util.hpp"
#include "../common/follower_file.hpp"
#include "../common/parser.hpp"

using namespace mp;
using namespace std;

struct group
{
    struct member
    {
        string name;
        double x = 0;
        double y = 0;
        double z = 0;
        vector<string> follows;

        template<typename Archive>
        void serialize(Archive &ar)
        {
            ar(cereal::make_nvp("name", name),
               cereal::make_nvp("x", x),
               cereal::make_nvp("y", y),
               cereal::make_nvp("z", z),
               cereal::make_nvp("follows", follows));
        }
    };

    string leader;
    vector<member> members;

    template<typename Archive>
    void serialize(Archive &ar)
    {
        ar(cereal::make_nvp("leader", leader));
        ar(cereal::make_nvp("members", members));
    }

    bool has_member(const string &name) const
    {
        return std::find_if(members.begin(), members.end(),
                            [&](const member &m) {
            return m.name == name;
        }) != members.end();
    }
};

void parse_options(int argc, char *argv[]);
void validate_options();

// Path to input follower file.
string in_file;

// Path to the correct manifest (for location data).
string manifest_file;

// The timestamp for graph generation.
bool absolute_timestamp = false;
i64  at_timestamp = 0;

// Path to output file.
string out_file;

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "C");
    parse_options(argc, argv);
    validate_options();

    // Load follower data from file
    following_data followers;
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
        load_follower_file(ar, followers, params);
    }

    tracing_data td;
    {
        scene_manifest sm = read_scene_manifest(manifest_file);
        if (sm.scene_type == "plain") {
            td = read_location_file(sm.get_plain_scene_data().data_file);
        } else if (sm.scene_type == "game") {
            td = read_location_file(sm.get_game_scene_data().location_file);
        } else {
            assert(false);
        }
    }

    cout << "Using follower data:\n"
         << "  Source:      " << in_file << "\n"
         << "  Manifest:    " << manifest_file << "\n"
         << "  Data source: " << params.data_source << "\n"
         << "  Algorithm:   " << params.algorithm << "\n"
         << "  Window size: " << params.window_size << " seconds\n"
         << "  Time lag:    " << params.time_lag << " seconds\n"
         << "  Devices:     " << followers.devices.size() << "\n"
         << "  Begin:       " << followers.begin_timestamp << "\n"
         << "  End:         " << followers.end_timestamp << "\n"
         << "  Duration:    " << followers.duration << " seconds\n";

    i64 abs_ts = absolute_timestamp ? at_timestamp
                                    : followers.begin_timestamp + at_timestamp;
    i64 rel_ts = abs_ts - followers.begin_timestamp;

    cout << "Computation parameters:\n"
         << "  Output:           " << out_file << "\n"
         << "  Timestamp (abs.): " << abs_ts << "\n"
         << "  Timestamp (rel.): " << rel_ts << "\n"
         << flush;

    if (abs_ts < followers.begin_timestamp || abs_ts > followers.end_timestamp) {
        cerr << "Timestamp " << abs_ts << " is out of range for follower data. Exiting." << endl;
        exit(1);
    } else if (abs_ts < td.min_timestamp || abs_ts > td.max_timestamp) {
        cerr << "Timestamp " << abs_ts << " is out of range for location data. Exiting." << endl;
        exit(1);
    }

    // Construct the graph and detect groups + their leaders.
    following_graph graph = following_graph_at(followers, abs_ts);
    // Finds the node for the given device or exits.
    auto find_vertex = [&](const string &name) {
        for (auto vertex : make_iter_range(vertices(graph))) {
            if (graph[vertex].name == name) {
                return vertex;
            }
        }
        cerr << "No vertex for name " << name << endl;
        exit(1);
    };

    vector<vector<string>> group_members = detect_groups(graph);
    vector<string> leaders = detect_leaders(graph, true);

    // Takes a device name and returns a group member,
    // including coordinates.
    auto make_member = [&](const string &name) {
        auto it = std::find_if(td.devices.begin(), td.devices.end(),
                               [&](const tracing_data::device_data &d) {
            return d.name == name;
        });
        if (it == td.devices.end()) {
            cerr << "No coordinates for device " << name << endl;
            exit(1);
        }
        array_view<const double> data = td.data_at(*it, abs_ts);

        auto vertex = find_vertex(name);

        group::member m;
        m.name = name;
        m.x = data[0];
        m.y = data[1];
        m.z = data[2];

        for (auto edge : make_iter_range(out_edges(vertex, graph))) {
            auto t = target(edge, graph);
            m.follows.push_back(graph[t].name);
        }
        return m;
    };

    // Gather location data for group members.
    vector<group> groups;
    for (auto &member_names : group_members) {
        group g;
        for (const string &member_name : member_names) {
            g.members.push_back(make_member(member_name));
        }

        groups.push_back(std::move(g));
    }
    for (auto &leader : leaders) {
        // Find the group that contains the leader
        auto it = std::find_if(groups.begin(), groups.end(),
                               [&](const group &g) {
            return g.has_member(leader);
        });
        if (it == groups.end()) {
            cerr << "No group for leader " << leader << endl;
            exit(1);
        }

        it->leader = leader;
    }

    // Save groups to file
    {
        fstream out_stream;
        try {
            try_open(out_stream, out_file, ios_base::out | ios_base::trunc);
        } catch (const std::exception &e) {
            cerr << "failed to open output file \""
                 << out_file << "\": "
                 << e.what() << endl;
            exit(1);
        }

        cereal::JSONOutputArchive ar(out_stream);
        ar(cereal::make_nvp("groups", groups));
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
            ("manifest",
             po::value<string>(&manifest_file)->value_name("PATH")->required(),
             "The path to the corrent scene manifest.\n"
             "This is needed for the location data of individual group members.")
            ("at",
              po::value<i64>(&at_timestamp)->value_name("TIMESTAMP")->required(),
             "Use the follower data at TIMESTAMP to generate the follower graph. "
             "By default, the timestamp is interpreted as relative from the beginning "
             "of the follower data (i.e. 0 -> first second).")
            ("absolute",
             po::bool_switch(&absolute_timestamp)->default_value(false),
             "Interpret the timestamp given for --at as absolute instead of relative.")
            ("output",
             po::value<string>(&out_file)->value_name("PATH")->required(),
             "The output path. The detected groups will be written to this file.")
            ;

    po::variables_map vm;
    try {
        auto parser = po::command_line_parser(argc, argv);
        parser.options(opts);
        po::store(parser.run(), vm);

        if (vm.count("help")) {
            cerr << "Usage: " << argv[0] << " [options]" << "\n";
            cerr << "\n";
            cerr << "This program takes a file previously created by the detect-followers\n"
                 << "program and extracts group data at some user-defined timestamp.\n"
                 << "A file will be created that stores the groups and their members\n"
                 << "as well as their spatial positions at the given timestamp.\n\n"
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

