#pragma once

#include "entire_collection.h"
#include <algorithm>
#include <vector>

/**
 * @brief Functional-like `map` function which returns a new std::vector
 *
 * @tparam T type to map from
 * @tparam Func
 * @param source
 * @param mapper a callable that transforms T into the desired type
 * @return std::vector a new std::vector<invoke_result_t<mapper>>
 */
template<typename T, typename Func>
auto map(const std::vector<T>& source, Func&& mapper)
{
    using Type = std::invoke_result_t<Func, T>;

    std::vector<Type> temp;
    std::transform(entire_collection(source), std::back_inserter(temp), mapper);
    return temp;
}