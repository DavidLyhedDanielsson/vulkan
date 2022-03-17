/**
 * @brief Small helper to tidy up the code, included in its own .h-file to make
 * sure it is possible to avoid global pollution
 */
#define entire_collection(x) std::begin(x), std::end(x)
