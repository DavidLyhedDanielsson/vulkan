template<typename T>
class IndexIterator
{
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = std::tuple<const typename T::value_type&, size_t>;
    using pointer = value_type*;
    using reference = value_type&;
    using iter = IndexIterator<T>;

    typename T::const_iterator lhs;
    size_t index;

  public:
    IndexIterator(const typename T::const_iterator& lhs, size_t index): lhs(lhs), index(index) {}

    bool operator==(const iter& other)
    {
        return index == other.index;
    }
    bool operator!=(const iter& other)
    {
        return !(*this == other);
    }

    value_type operator*()
    {
        return value_type(*lhs, index);
    }

    iter operator++()
    {
        lhs++;
        index++;
        return *this;
    }

    iter operator++(int)
    {
        auto temp = *this;
        ++(*this);
        return temp;
    }
};

/**
 * @brief A function which takes a collection and returns an iterator of std::tuple<T, size_t> where
 * size_t corresponds to the current iteration index.
 *
 * For instance:
 * for(auto [val, index]: Index(collection)) {
 *     // index will be 0, 1, 2...
 * }
 *
 * @tparam T
 */
template<typename T>
struct Index
{
    // I'm not a huge fan of the name `Index` but it's alright for now
    Index(const T& lhs): lhs(lhs) {}

    auto begin() const
    {
        return IndexIterator<T>(lhs.begin(), 0);
    }
    auto end() const
    {
        return IndexIterator<T>(lhs.end(), lhs.size());
    }

  private:
    const T& lhs;
};
