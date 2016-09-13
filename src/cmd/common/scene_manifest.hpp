#ifndef COMMON_SCENE_MANIFEST_HPP
#define COMMON_SCENE_MANIFEST_HPP

#include <unordered_map>

#include <boost/variant.hpp>

#include "mp/defs.hpp"

struct plain_scene_data
{
    std::string data_file;           // path to location or signal file
    std::string ground_truth_file;   // path to ground truth file (can be empty)
};

struct game_scene_data
{
    std::string folder; // path to signal & ground-truth folder
    std::unordered_map<mp::string, mp::i32> evaders; // set of evaders
    std::string location_file; // only required if data_type == location

    bool is_evader(const mp::string &device) const
    {
        return evaders.count(device) != 0;
    }
};

// The game manifest contains required information
// to parse a game experiment.
struct scene_manifest
{
    mp::string name;                   // name of the scene
    mp::string scene_type;             // "plain" or "game"
    mp::string data_type;              // "signal" or "location"
    mp::i64 start;                     // first timestamp
    mp::i64 end;                       // last timestamp (incl.)
    mp::vector<mp::string> targets;   // list of device-ids

    // Type depends on the "scene_type" field
    boost::variant<plain_scene_data, game_scene_data> data;

    // Load manifest from file.
    void load(std::istream &i);

    // Validate this manifest.
    // Writes error messages into "errors".
    bool validate(std::ostream &errors);

    // These functions throw if the manifest does not describe the query type.
    plain_scene_data& get_plain_scene_data() { return boost::get<plain_scene_data>(data); }
    game_scene_data& get_game_scene_data() { return boost::get<game_scene_data>(data); }
    const plain_scene_data& get_plain_scene_data() const { return boost::get<plain_scene_data>(data); }
    const game_scene_data& get_game_scene_data() const { return boost::get<game_scene_data>(data); }
};



#endif // COMMON_SCENE_MANIFEST_HPP
