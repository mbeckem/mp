#ifndef MP_SERIALIZE_FEATURES_HPP
#define MP_SERIALIZE_FEATURES_HPP

#include <sstream>

#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/details/traits.hpp>
#include <cereal/external/base64.hpp>

#include "defs.hpp"
#include "tools/array_2d.hpp"
#include "tools/iter.hpp"

namespace mp {

// A type that either serializes to a length prefixed
// binary string or a base64 string, depending
// on whether the Archive supports binary data.
struct binary_string { string value; };

template<typename Archive>
typename std::enable_if<cereal::traits::is_output_serializable<cereal::BinaryData<char>, Archive>::value, void>::type
save(Archive &ar, const binary_string &b)
{
    u64 size = b.value.size();
    ar(cereal::make_size_tag(size));
    ar(cereal::binary_data(&b.value[0], b.value.size()));
}

template<typename Archive>
typename std::enable_if<cereal::traits::is_input_serializable<cereal::BinaryData<char>, Archive>::value, void>::type
load(Archive &ar, binary_string &b)
{
    u64 size;
    ar(cereal::make_size_tag(size));
    b.value.resize(static_cast<size_t>(size));
    ar(cereal::binary_data(&b.value[0], b.value.size()));
}

template<typename Archive>
typename std::enable_if<!cereal::traits::is_output_serializable<cereal::BinaryData<char>, Archive>::value, string>::type
save_minimal(const Archive &, const binary_string &b)
{
    return base64::encode(reinterpret_cast<const unsigned char*>(b.value.data()), b.value.size());
}

template<typename Archive>
typename std::enable_if<!cereal::traits::is_input_serializable<cereal::BinaryData<char>, Archive>::value, void>::type
load_minimal(const Archive &, binary_string &b, const string &value)
{
    b.value = base64::decode(value);
}

template<typename Archive, typename Iter>
void load(Archive &ar, iter_range<Iter> &range)
{
    // Read the unused size.
    // The size must be equal to the real size of the range.
    // The real size was already known by other means (e.g. rows * columns in array_2d)
    // and is only validated in debug mode.
    // Iterator ranges cannot be resized, anyway.
    u64 size;
    ar(cereal::make_size_tag(size));
    assert(size == std::distance(range.begin(), range.end()));

    for (auto &e : range) {
        ar(e);
    }
}

template<typename Archive, typename Iter>
void save(Archive &ar, const iter_range<Iter> &range)
{
    // The size is only included to get array notation in json format.
    // It will not be used by the loading function, other than asserting 
    // that the size is correct.
    u64 size = std::distance(range.begin(), range.end());
    ar(cereal::make_size_tag(size));
    for (const auto &e : range) {
        ar(e);
    }
}

template<typename Archive, typename T>
void save(Archive &ar, const array_2d<T> &array)
{
    ar(cereal::make_nvp("rows", u64(array.rows())),
       cereal::make_nvp("columns", u64(array.columns())));

    auto range = make_iter_range(array.begin(), array.end());
    ar(cereal::make_nvp("data", range));
}

template<typename Archive, typename T>
void load(Archive &ar, array_2d<T> &array)
{
    u64 rows, columns;
    ar(cereal::make_nvp("rows", rows));
    ar(cereal::make_nvp("columns", columns));

    array.resize(rows, columns);
    auto range = make_iter_range(array.begin(), array.end());
    ar(cereal::make_nvp("data", range));
}

} // namespace mp

#endif // MP_SERIALIZE_FEATURES_HPP

