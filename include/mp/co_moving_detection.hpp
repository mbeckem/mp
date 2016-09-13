#ifndef MP_CO_MOVING_DETECTION_HPP
#define MP_CO_MOVING_DETECTION_HPP

#include <memory>

#include "defs.hpp"
#include "serialization.hpp"
#include "tools/array_view.hpp"

namespace mp {

struct ground_truth;
struct similarity_data;
struct tracing_data;

/**
 * Classifies feature vectors as either co-moving or not co-moving.
 *
 * A classifier can be created by learning from a data set
 * of both feature vectors and ground truth.
 * Once created, it can be saved and loaded to a file.
 * 
 * It is implemented using a support vector machine.
 */
class co_moving_classifier
{
public:
    static void print_cross_validation(const similarity_data &data, const ground_truth &gt, std::ostream &o);

public:
    /**
     * Returns true if the feature vector a is classified as "co-moving".
     * \p a should be the feature vector of a pair of devices.
     * It should have been computed with the same method as the machine's learning data
     * (i.e. time lag, window size and algorithm).
     *
     * Please note that this function is not thread-safe,
     * since it uses an internal, mutable buffer.
     */
    bool co_moving(array_view<const double> a);

    /**
     * Learn from the data set and ground truth.
     * Replaces the state of this classifier.
     * Future classifications in co_moving() will be based
     * on this learning data.
     */
    void learn(const similarity_data &data, const ground_truth &gt);

    /**
     * Load the classifier from the given stream,
     * replacing its state.
     */
    void load(std::istream &i);

    /**
     * Save the classifier to the given stream.
     */
    void save(std::ostream &o) const;

    co_moving_classifier();
    co_moving_classifier(co_moving_classifier &&);
    ~co_moving_classifier();

    co_moving_classifier& operator =(co_moving_classifier &&);
private:
    struct impl;

private:
    std::unique_ptr<impl> m_impl;
};

/**
 * Save the classifier to the archive.
 *
 * \relates co_moving_classifier
 */
template<typename Archive>
void save(Archive &ar, const co_moving_classifier &c)
{
    // Serialize "c" to a binary string first.
    std::ostringstream out;
    c.save(out);

    binary_string binary{out.str()};
    ar(cereal::make_nvp("data", binary));
}

/**
 * Load a classifier from the archive.
 *
 * \relates co_moving_classifier
 */
template<typename Archive>
void load(Archive &ar, co_moving_classifier &c)
{
    // Deserialize the data string.
    binary_string binary;
    ar(cereal::make_nvp("data", binary));

    std::istringstream in(binary.value);
    c.load(in);
}

} // namespace mp

#endif // MP_CO_MOVING_DETECTION_HPP
