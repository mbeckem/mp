#ifndef COMMON_PARSER_HPP
#define COMMON_PARSER_HPP

#include "mp/ground_truth.hpp"
#include "mp/tracing_data.hpp"

#include "scene_manifest.hpp"

// Read a game manifest from file and valiadate.
// Exits on error.
scene_manifest read_scene_manifest(const mp::string &path);

// Reads a plain signal file. Exits on error.
mp::tracing_data read_signal_file(const mp::string &path,
                              double minimum_average,
                              mp::i32 missing_reading);

// Reads a plain location file. Exits on error.
mp::tracing_data read_location_file(const mp::string &path);

// Reads a plain ground truth file. Exits on error.
mp::ground_truth read_ground_truth_file(const mp::string &path);

// Reads game signal files. Exits on error.
mp::tracing_data read_game_signal_files(const scene_manifest &gm,
                                         double minimum_average,
                                         mp::i32 missing_reading);

// Reads game ground truth files. Exits on error.
mp::ground_truth read_game_ground_truth(const scene_manifest &gm);

#endif // COMMON_PARSER_HPP
