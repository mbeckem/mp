#include "mp/parser.hpp"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace mp {

namespace {

using access_points_map = std::unordered_map<string, i32>;
using devices_map       = std::unordered_map<string, i32>;

// Finds the unique index of the given access point (via its BSSID).
// If the access point was not seen yet, it is inserted into both "bssids" and "aps".
i32 get_access_point_index(access_points_map &aps, const string &bssid, vector<string> &bssids)
{
    auto it = aps.find(bssid);
    if (it == aps.end()) {
        i32 index = bssids.size();
        aps.emplace(bssid, index);
        bssids.emplace_back(bssid);
        return index;
    }
    return it->second;
}

// Finds or creates a new device_data instance and returns a reference to it (signal data).
signal_data::device_data& get_device(devices_map &devs, const string &name, vector<signal_data::device_data> &devices)
{
    auto it = devs.find(name);
    if (it == devs.end()) {
        i32 index = devices.size();
        devs.emplace(name, index);
        devices.emplace_back(name);
        return devices.back();
    }
    return devices[it->second];
}

// Finds or creates a new device_data instance and returns a reference to it (location data).
location_data::device_data& get_device(devices_map &devs, const string &name, vector<location_data::device_data> &devices)
{
    auto it = devs.find(name);
    if (it == devs.end()) {
        i32 index = devices.size();
        devs.emplace(name, index);
        devices.emplace_back(name);
        return devices.back();
    }
    return devices[it->second];
}

// Split on "delim", output into "token".
// Returns false if the end of the line was reached.
// The end of line is treated as a delimiter.
bool next_token(const string &input, string::const_iterator &pos, char delim, string &token)
{
    token.clear();

    auto end = input.end();
    if (pos == end) {
        return false;
    }

    for (; pos != end; ++pos) {
        if (*pos == delim) {
            ++pos;
            break;
        }
        token += *pos;
    }

    return true;
}

// Calls next_token and throws if it returns false.
// Otherwise, returns a reference to "token".
string& must_next_token(const string &input, string::const_iterator &pos,
                        char delim, string &token, const char *context)
{
    if (!next_token(input, pos, delim, token)) {
        std::string message = "invalid signal data (expected a token)";
        if (context) {
            message += " in context `";
            message += context;
            message += "`";
        }
        throw std::runtime_error(message);
    }
    return token;
}

// Reads rows of signal data into the "result" parameter.
// Uses "aps" and "devs" as a lookup cache for access point/device indices.
void parse_signal_stream(std::istream &file, signal_data &result, access_points_map &aps, devices_map &devs)
{
    string line, token;
    string::const_iterator pos, inner_pos;

    i64 timestamp;
    string device_id, bssid, dbm;
    while (std::getline(file, line)) {
        pos = line.cbegin();

        timestamp = std::stoll(must_next_token(line, pos, ';', token, "TIMESTAMP"));
        device_id = must_next_token(line, pos, ';', token, "DEVICE_ID");

        auto &device = get_device(devs, device_id, result.devices);
        while (next_token(line, pos, ';', token)) {
            if (token == "pos=" || token == "id=") {
                // Some lines in the input data are invalid, they look like this:
                // 1305645282;B00056;pos=;id=
                continue;
            }

            // A repeating field that contains
            // 1. the access point BSSID
            // 2. "=" followed by a dBm value
            // 3. "," followed by a frequency (ignored)
            // -- two other values i don't recognize (ignored)
            inner_pos = token.cbegin();
            must_next_token(token, inner_pos, '=', bssid, "BSSID");
            must_next_token(token, inner_pos, ',', dbm, "DBM");
            // ignore rest of token

            i32 ap_index = get_access_point_index(aps, bssid, result.bssids);
            device.data.push_back({timestamp, ap_index, std::stoi(dbm)});
        }
    }

    for (auto &dev : result.devices) {
        // Make sure that the entries are sorted by time.
        // This should already be the case given the file structure,
        // but isn't documented anywhere.
        using measure = signal_data::measurement;
        std::sort(dev.data.begin(), dev.data.end(),
                  [](const measure &a, const measure &b) { return a.timestamp < b.timestamp; });
    }
}

void parse_location_stream(std::istream &file, location_data &result, devices_map &devs)
{
    string line, token;
    string::const_iterator pos;

    location_data::measurement next;
    while (std::getline(file, line)) {
        // Line format:
        // [('ts','int'),('user','str'),('lat','float'),('long','float'),('alt','float'),
        //  ('uncertainty','float'),('speed','float'),('heading','float'),('vspeed','float')]

        pos = line.cbegin();

        next.timestamp = std::stoll(must_next_token(line, pos, ';', token, "TIMESTAMP"));

        must_next_token(line, pos, ';', token, "DEVICE_ID");
        auto &dev = get_device(devs, token, result.devices);

        next.lat            = std::stod(must_next_token(line, pos, ';', token, "LAT"));
        next.lng            = std::stod(must_next_token(line, pos, ';', token, "LNG"));
        next.alt            = std::stod(must_next_token(line, pos, ';', token, "ALT"));
        next.uncertainty    = std::stod(must_next_token(line, pos, ';', token, "UNCERTAINTY"));
        next.speed          = std::stod(must_next_token(line, pos, ';', token, "SPEED"));
        next.heading        = std::stod(must_next_token(line, pos, ';', token, "HEADING"));
        next.vspeed         = std::stod(must_next_token(line, pos, ';', token, "VSPEED"));

        dev.data.push_back(next);
    }

    for (auto &dev : result.devices) {
        using measure = location_data::measurement;
        std::sort(dev.data.begin(), dev.data.end(),
                  [](const measure &a, const measure &b) { return a.timestamp < b.timestamp; });
    }
}

void parse_ground_truth_stream(std::istream &input, ground_truth &result)
{
    using device_list = vector<ground_truth::device>;

    string line, token, inner_token;
    string::const_iterator line_pos, token_pos;

    i32 group = 0;
    i64 start, end;
    device_list devices;
    while (std::getline(input, line)) {
        // The format of the ground truth files:
        // Follower <SOME NUMBER> <HUMAN_READABLE_START> <HUMAN_READABLE_END> <START> <END> <LIST_OF_DEVICES>...
        // The list of devices is ordered, the first one is the leader of the group.
        // Devices separated by "," instead of " " are moving next to each other and
        // not in a row.
        line_pos = line.begin();
        if (line.empty() || line[0] == '#') {
            continue;
        }

        must_next_token(line, line_pos, ' ', token, "FOLLOWER");
        std::transform(token.begin(), token.end(), token.begin(),
                       [](char c) { return std::toupper(c); });
        if (token != "FOLLOWER") {
            throw std::runtime_error("Expected a line starting with 'FOLLOWER'");
        }

        // Skip 3 tokens
        must_next_token(line, line_pos, ' ', token, "NUMBER");
        must_next_token(line, line_pos, ' ', token, "HUMAN_READABLE_START");
        must_next_token(line, line_pos, ' ', token, "HUMAN_READABLE_END");

        // Begin and end timestamp
        start = std::stoll(must_next_token(line, line_pos, ' ', token, "START"));
        end   = std::stoll(must_next_token(line, line_pos, ' ', token, "END"));

        i32 order = 0;
        while (next_token(line, line_pos, ' ', token)) {
            if (token == "SPACE" || token == "") {
                continue;
            }

            // All following tokens are either a single device ID or
            // a list of device IDs separated by ","
            if (token.find(',')  != string::npos) {
                token_pos = token.begin();
                while (next_token(token, token_pos, ',',  inner_token)) {
                    // Devices separated by "," have the same order.
                    devices.push_back({inner_token, group, order});
                }
            } else {
                devices.push_back({token, group, order});
            }
            ++order;
        }

        for (i64 ts = start; ts <= end; ++ts) {
            device_list &target = result.timestamps[ts];
            target.reserve(target.size() + devices.size());

            std::copy(devices.begin(), devices.end(),
                      std::back_inserter(target));
        }


        devices.clear();
        ++group;
    }
}

} // namespace

signal_data parse_signal_data(std::istream &input)
{
    /*
     * Signal data files contain csv-like data, but do not
     * (appear to) use escape sequences or quotes.
     * I can get away with not using a complete csv library.
     */
    signal_data       result;
    access_points_map aps;    // bssid => index
    devices_map       devs;   // name  => index

    parse_signal_stream(input, result, aps, devs);
    return result;
}

location_data parse_location_data(std::istream &input)
{
    location_data result;
    devices_map     devs; // name => index

    parse_location_stream(input, result, devs);
    return result;
}

ground_truth parse_ground_truth_data(std::istream &input)
{
    ground_truth result;
    parse_ground_truth_stream(input, result);
    return result;
}

void game_signal_data_parser::parse(const string &device_id, std::istream &input)
{
    auto &device = get_device(m_devices, device_id, m_result.devices);

    string line, token;
    string::const_iterator line_pos, token_pos;

    i64 timestamp;
    string bssid, dbm;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        line_pos = line.cbegin();

        timestamp = std::stoll(must_next_token(line, line_pos, ';', token, "TIMESTAMP"));
        timestamp /= 1000; // milliseconds

        must_next_token(line, line_pos, ';', token, "DEVICE_ID");
        assert(token == device_id); // one device per file

        while (next_token(line, line_pos, ';', token)) {
            // Pretty much the same as in parse_signal_stream.
            token_pos = token.cbegin();
            must_next_token(token, token_pos, '=', bssid, "BSSID");
            must_next_token(token, token_pos, ',', dbm, "DBM");
            // ignore rest of token

            i32 ap_index = get_access_point_index(m_aps, bssid, m_result.bssids);
            device.data.push_back({timestamp, ap_index, std::stoi(dbm)});
        }
    }
}

signal_data game_signal_data_parser::take()
{
    auto data = std::move(m_result);
    m_result = signal_data();

    for (auto &dev : data.devices) {
        using measure = signal_data::measurement;
        std::sort(dev.data.begin(), dev.data.end(),
                  [](const measure &a, const measure &b) { return a.timestamp < b.timestamp; });
    }
    return data;
}

game_ground_truth_parser::game_ground_truth_parser(const evader_map &evaders,
                                                   i64 timestamp_begin,
                                                   i64 timestamp_end)
    : m_evaders(evaders)
    , m_next_id(0)
    , m_begin(timestamp_begin)
    , m_end(timestamp_end)
    , m_gt()
{
    if (timestamp_begin > timestamp_end) {
        throw std::logic_error("timestamp_begin must be <= timestamp_end");
    }

    // Every evader gets its own group for the entire duration of the experiment.
    for (auto &pair : m_evaders) {
        const string &device_id = pair.first;
        i32 evader_id = pair.second;

        // Evader ids must be unique.
        bool inserted;
        std::tie(std::ignore, inserted) = m_evader_id_set.insert(evader_id);
        if (!inserted) {
            throw std::logic_error("duplicate evader id: " + std::to_string(evader_id));
        }

        // Free IDs that can be used as follower-group indices when they move alone.
        m_next_id = std::max(m_next_id, evader_id + 1);

        // Use evader index as group index.
        ground_truth::device dev{device_id, evader_id, 0};
        for (i64 ts = m_begin; ts <= m_end; ++ts) {
            m_gt.timestamps[ts].push_back(dev);
        }
    }
}

void game_ground_truth_parser::parse(const string &device_id, std::istream &input)
{
    using device = ground_truth::device;

    i32 unique_id = m_next_id++;

    if (m_evaders.count(device_id)) {
        // device is evader, we already know its ground truth (see constructor).
        return;
    }

    string line, token;
    string::const_iterator line_pos;

    i64 last_timestamp = m_begin;
    i32 last_evader_id = -1;
    while (std::getline(input, line)) {
        line_pos = line.begin();

        must_next_token(line, line_pos, ';', token, "TIMESTAMP");
        if (token == "timestamp") {
            continue; // first line of a followevent file.
        }

        i64 timestamp = std::stoll(token) / 1000; // milliseconds
        if (timestamp < m_begin) {
            continue;
        } else if (timestamp > m_end) {
            break;
        }

        must_next_token(line, line_pos, ';', token, "EVADER_ID");
        i32 evader_id = std::stoi(token);

        if (last_timestamp > timestamp) {
            throw std::runtime_error("timestamps must be sorted (ascending)");
        }

        // Unique group if not following an evader,
        // otherwise at order 1 in evader's group.
        i32 group_id = last_evader_id != -1 ? last_evader_id : unique_id;
        i32 group_order = last_evader_id != -1 ? 1 : 0;
        device d{device_id, group_id, group_order};
        for (i64 ts = last_timestamp; ts < timestamp; ++ts) {
            m_gt.timestamps[ts].push_back(d);
        }

        last_timestamp = timestamp;
        last_evader_id = evader_id;
    }

    i32 group_id = last_evader_id != -1 ? last_evader_id : unique_id;
    i32 group_order = last_evader_id != -1 ? 1 : 0;
    device d{device_id, group_id, group_order};
    for (i64 ts = last_timestamp; ts <= m_end; ++ts) {
        m_gt.timestamps[ts].push_back(d);
    }
}

ground_truth game_ground_truth_parser::take()
{
    auto data = std::move(m_gt);
    m_gt = ground_truth();
    return data;
}

} // namespace mp
