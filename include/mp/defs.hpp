#ifndef MP_DEFS_HPP
#define MP_DEFS_HPP

#include <cassert>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <tuple>
#include <vector>

#include <boost/numeric/conversion/cast.hpp>

/// The main namespace for this library.
namespace mp {

using std::size_t;
using std::string;

using std::pair;
using std::tuple;
using std::make_tuple;
using std::get;

using std::vector;

using i32 = int32_t;
using i64 = int64_t;

using u32 = uint32_t;
using u64 = uint64_t;

using boost::numeric_cast;

} // namespace mp

#endif // MP_DEFS_HPP
