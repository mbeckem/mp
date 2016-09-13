#include "scene_manifest.hpp"

#include <iostream>
#include <sstream>

#define PICOJSON_USE_INT64
#include <picojson/picojson.h>

namespace pj = picojson;

template<typename T>
T& get_as(pj::value &v, const char *v_name, const char *t_name)
{
    if (!v.is<T>()) {
        std::ostringstream s;
        s << v_name << " is not of type " << t_name;
        throw std::runtime_error(s.str());
    }
    return v.get<T>();
}

template<typename T>
T& get_member_as(pj::object &o, const char *v_name, const char *t_name)
{
    return get_as<T>(o[v_name], v_name, t_name);
}

void scene_manifest::load(std::istream &i)
{
    pj::value v;
    {
        std::string err = pj::parse(v, i);
        if (!err.empty()) {
            throw std::runtime_error(err);
        }
    }

    pj::object &root = get_as<pj::object>(v, "root", "object");
    this->name = get_member_as<std::string>(root, "name", "string");
    this->scene_type = get_member_as<std::string>(root, "scene_type", "string");
    this->data_type = get_member_as<std::string>(root, "data_type", "string");
    this->start = get_member_as<mp::i64>(root, "start", "integer");
    this->end = get_member_as<mp::i64>(root, "end", "integer");

    this->targets.clear();
    pj::array &targets = get_member_as<pj::array>(root, "targets", "array");
    std::transform(targets.begin(), targets.end(), std::back_inserter(this->targets),
                   [](pj::value &json_target) {
        return get_as<std::string>(json_target, "target", "string");
    });


    pj::object &data = get_member_as<pj::object>(root, "data", "object");
    if (this->scene_type == "plain") {
        plain_scene_data d;
        d.data_file = get_member_as<std::string>(data, "data_file", "string");

       pj::value &gt_value = data["ground_truth_file"];
        if (!gt_value.is<picojson::null>()) {
            // Optional
            d.ground_truth_file = get_as<std::string>(gt_value, "ground_truth_file", "string");
        }

        this->data = std::move(d);
    } else if (this->scene_type == "game") {
        game_scene_data d;
        d.folder = get_member_as<std::string>(data, "folder", "string");

        pj::object &evaders = get_member_as<pj::object>(data, "evaders", "object");
        for (auto &pair : evaders) {
            d.evaders[pair.first] = get_as<mp::i64>(pair.second, "evader-id", "integer");
        }

        pj::value &loc_value = data["location_file"];
        if (!loc_value.is<picojson::null>()) {
            // Only required for "location"
            d.location_file = get_as<std::string>(loc_value, "location_file", "string");
        }

        this->data = std::move(d);
    }
}

bool scene_manifest::validate(std::ostream &errors)
{
    bool ok = true;
    if (start < 0) {
        ok = false;
        errors << "Invalid start time: " << start << std::endl;
    }
    if (end < start) {
        ok = false;
        errors << "Invalid end time: " << end << std::endl;
    }
    if (name == "") {
        ok = false;
        errors << "Scene name is empty: " << std::endl;
    }
    if (data_type != "signal" && data_type != "location") {
        ok = false;
        errors << "Unsupported data type: " << data_type << std::endl;
    }
    if (scene_type != "game" && scene_type != "plain") {
        ok = false;
        errors << "Unsupported scene type: " << scene_type << std::endl;
    }
    if (targets.empty()) {
        ok = false;
        errors << "No targets specified" << std::endl;
    }

    if (scene_type == "plain") {
        plain_scene_data &d = boost::get<plain_scene_data>(data);
        if (d.data_file == "") {
            ok = false;
            errors << "No data file specified" << std::endl;
        }
        // ground truth optional
    } else if (scene_type == "game") {
        game_scene_data &d = boost::get<game_scene_data>(data);
        if (d.folder == "") {
            ok = false;
            errors << "No folder specified" << std::endl;
        }

        if (d.evaders.empty()) {
            ok = false;
            errors << "No evaders specified" << std::endl;
        }
        for (auto &pair : d.evaders) {
            const std::string &t = pair.first;
            if (std::find(targets.begin(), targets.end(), t) == targets.end()) {
                ok = false;
                errors << "evader is not in targets: " << t << std::endl;
            }
        }

        if (data_type == "location" && d.location_file == "") {
            ok = false;
            errors << "No location file specified" << std::endl;
        }
    }

    return ok;
}
