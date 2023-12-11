#pragma once

#include <TypeTraits.h>
#include <any>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include <QHash>
#include <QString>

namespace QtNodes {

class FromToTypes
{
public:
    FromToTypes(QString from, QString to)
        : m_from(std::move(from))
        , m_to(std::move(to))
    {}

    const QString &from() const noexcept { return m_from; }

    const QString &to() const noexcept { return m_to; }

    bool operator==(const FromToTypes &other) const noexcept
    {
        return (m_from == other.m_from) && (m_to == other.m_to);
    }

private:
    QString m_from;
    QString m_to;
};

template<class T>
inline void hash_combine(std::size_t &seed, const T &v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}
} //namespace QtNodes

namespace std {
template<>
struct hash<QtNodes::FromToTypes>
{
    size_t operator()(const QtNodes::FromToTypes &from_to_types) const noexcept
    {
        size_t seed = 0;
        QtNodes::hash_combine(seed, from_to_types.from());
        QtNodes::hash_combine(seed, from_to_types.to());
        return seed;
    }
};
} // namespace std

namespace QtNodes {

using ConverterFunction = std::function<std::any(const std::any &)>;

class ConvertersRegister : public std::unordered_map<FromToTypes, ConverterFunction>
{
public:
    ConvertersRegister() = default;

    ConvertersRegister(const ConvertersRegister &converters) = default;

    ConvertersRegister(ConvertersRegister &&converters) noexcept = default;

    ConvertersRegister &operator=(const ConvertersRegister &converters) = default;

    template<typename Func>
    void add_converter(Func)
    {
        constexpr size_t arg_count = StdExt::ArgCount_v<Func>;
        static_assert(arg_count == 1, "Converter function must have exactly one argument");
        using ToType = StdExt::ReturnType_t<Func>;
        using FromType = StdExt::ArgType_t<Func, 0>;
        using FromTypeWithoutReference = std::remove_reference_t<FromType>;
        const FromToTypes from_to_types{ToType::id(), FromTypeWithoutReference::id()};
        this->emplace(from_to_types, [](const std::any &any_from) -> std::any {
            const FromType &from_casted = std::any_cast<FromType>(any_from);
            return Func()(from_casted);
        });
    }
};

} // namespace QtNodes
