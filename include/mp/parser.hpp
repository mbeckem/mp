#ifndef MP_PARSER_HPP
#define MP_PARSER_HPP

#include "defs.hpp"
#include "ground_truth.hpp"
#include "location_data.hpp"
#include "signal_data.hpp"

namespace mp {

/**
 * Parses signal data files.
 */
signal_data parse_signal_data(std::istream &input);

/**
 * Parses location data files.
 */
location_data parse_location_data(std::istream &input);

/**
 * Parses ground truth files for scipted scenes.
 */
ground_truth parse_ground_truth_data(std::istream &input);

/**
 * Parses game signal strength data files.
 * This needs to be a class because a game experiment
 * consists of multiple signal strength files (one for each device).
 * The files can be pushed into the result by calling `parse()`.
 */
class game_signal_data_parser
{
public:
    /**
     * Parse signal data from `input` into the result.
     * The data in `input` must belong to the device called `device_id`.
     */
    void parse(const string &device_id, std::istream &input);

    /**
     * Returns the signal_data instance that contains
     * all parsed data (up to this point).
     * Calling `take()` clears the internal instance, it will be empty.
     */
    signal_data take();

private:
    using access_points_map = std::unordered_map<string, i32>;
    using devices_map       = std::unordered_map<string, i32>;

    access_points_map m_aps;
    devices_map m_devices;
    signal_data m_result;
};

/**
 * Parses game ground truth files (".followevent").
 * Data should be pushed into the parser with `parse()`.
 * The result can be retrieved by calling `take()`.
 */
class game_ground_truth_parser
{
public:
    /**
     * A evader_map maps a numeric integer to a device id.
     * The integers must be > 0.
     */
    using evader_map = std::unordered_map<string, i32>;

public:
    /**
     * Construct a parser with a map of evaders
     * and a time range. Following-events not in this time
     * range will be ignored.
     * Both timestamps are inclusive.
     */
    game_ground_truth_parser(const evader_map &evaders,
                             i64 timestamp_begin,
                             i64 timestamp_end);

    /**
     * Parse ground truth data from the given input stream.
     * The data must belong to the device called "device_id".
     */
    void parse(const string &device_id, std::istream &input);

    /**
     * Returns the ground truth data that contains all parsed
     * information up to this point.
     * The parser's ground truth will be reset.
     */
    ground_truth take();

private:
    using evader_set = std::unordered_set<i32>;

private:
    evader_map m_evaders;
    evader_set m_evader_id_set;
    i32 m_next_id;
    i64 m_begin;
    i64 m_end;
    ground_truth m_gt;
};

} // namespace mp

#endif // MP_PARSER_HPP
