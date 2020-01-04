//          Copyright Adam Lach 2017
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

//local
#include <separator.hpp>

//boost
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/utility/string_ref.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/functional/hash.hpp>

//std
#include <memory>
#include <utility>
#include <vector>
#include <string>
#include <functional>
#include <cstring>
#include <limits>

namespace dconfig {
namespace detail {

class ConfigNode : public std::enable_shared_from_this<ConfigNode>
{
    struct sequenced {};
    struct ordered {};
    struct referenced {};

    template<typename List>
    struct ElementType
    {
        using list_type = List;

        struct HashByKeyRef : std::unary_function<boost::string_ref, size_t>
        {
            std::size_t operator()(const boost::string_ref& value) const
            {
                return boost::hash_range(value.begin(), value.end());
            }
        };

        boost::string_ref keyRef() const { return boost::string_ref(key); }

        std::string key;
        list_type value;
    };

    template<typename List>
    using children_container = boost::multi_index::multi_index_container
        < ElementType<List>
        , boost::multi_index::indexed_by
            < boost::multi_index::ordered_unique
                < boost::multi_index::tag<ordered>
                , boost::multi_index::member<ElementType<List>, std::string, &ElementType<List>::key>>
            , boost::multi_index::hashed_unique
                < boost::multi_index::tag<referenced>
                , boost::multi_index::const_mem_fun<ElementType<List>, boost::string_ref, &ElementType<List>::keyRef>
                , typename ElementType<List>::HashByKeyRef>
            , boost::multi_index::sequenced
                < boost::multi_index::tag<sequenced>> >>;

public:
    using value_type = std::string;
    using node_type  = std::shared_ptr<ConfigNode>;

    using value_list = std::vector<value_type>;
    using node_list  = std::vector<node_type>;

    template<typename S>
    friend S& operator<<(S&& stream, const ConfigNode& node)
    {
        node.print(stream, "");
        return stream;
    }

    bool empty() const
    {
        return values.empty() && nodes.empty();
    }

    template<typename T>
    void setValue(const std::string& key, T&& value, size_t index = std::numeric_limits<size_t>::max())
    {
        auto&& it = values.get<ordered>().find(key);
        if (it == values.get<ordered>().end())
        {
            values.get<sequenced>().push_back({key, value_list{std::forward<T>(value)}});
        }
        else
        {
            values.get<ordered>().modify(it,
                [&value, index](ElementType<value_list>& element)
                {
                    if (index == std::numeric_limits<size_t>::max())
                    {
                        element.value.emplace_back(std::forward<T>(value));
                    }
                    else
                    {
                        assert(element.value.size() > index);
                        element.value[index] = std::forward<T>(value);
                    }
                });
        }
    }

    template<typename T>
    void setNode(const std::string& key, T&& node, size_t index = std::numeric_limits<size_t>::max())
    {
        node->parent = shared_from_this();
        auto&& it = nodes.get<ordered>().find(key);
        if (it == nodes.get<ordered>().end())
        {
            nodes.get<sequenced>().push_back({key, node_list{std::forward<T>(node)}});
        }
        else
        {
            nodes.get<ordered>().modify(it,
                [&node, index](ElementType<node_list>& element)
                {
                    if (index == std::numeric_limits<size_t>::max())
                    {
                        element.value.emplace_back(std::forward<T>(node));
                    }
                    else
                    {
                        assert(element.value.size() > index);
                        element.value[index] = std::forward<T>(node);
                    }
                });
        }
    }

    const value_list& getValues(const char* key, Separator separator) const
    {
        if (key)
        {
            clearJustSeparatorKey(key, separator);
            if (auto&& first = std::strchr(key, separator))
            {
                auto&& nodes = this->getNodes(boost::string_ref(key, first - key));
                if (!nodes.empty())
                {
                    return nodes[0]->getValues(first + 1, separator);
                }
            }
            else
            {
                return this->getValues(boost::string_ref(key, std::strlen(key)));
            }
        }

        return noValues();
    }

    const value_list& getValues(const std::string& key, Separator separator) const
    {
        return this->getValues(key.c_str(), separator);
    }

    template<typename... K>
    const value_list& getValues(const boost::string_ref& key1, const boost::string_ref& key2, K&&... keys) const
    {
        auto&& nodes = getNodes(key1);
        return (!nodes.empty())
            ? nodes[0]->getValues(key2, keys...)
            : noValues();
    }

    const value_list& getValues(const boost::string_ref& key) const
    {
        auto&& child = values.get<referenced>().find(key);
        if (child != values.get<referenced>().end())
        {
            return child->value;
        }

        return noValues();
    }

    const node_list& getNodes(const char* key, Separator separator) const
    {
        if (key)
        {
            clearJustSeparatorKey(key, separator);
            if (auto&& first = std::strchr(key, separator))
            {
                auto&& nodes = this->getNodes(boost::string_ref(key, first - key));
                if (!nodes.empty())
                {
                    return nodes[0]->getNodes(first + 1, separator);
                }
            }
            else
            {
                return this->getNodes(boost::string_ref(key, std::strlen(key)));
            }
        }

        return noNodes();
    }

    const node_list& getNodes(const std::string& key, Separator separator) const
    {
        return getNodes(key.c_str(), separator);
    }

    template<typename... K>
    const node_list& getNodes(const boost::string_ref& key1, const boost::string_ref& key2, K&&... keys) const
    {
        auto&& nodes = getNodes(key1);
        return (!nodes.empty())
            ? nodes[0]->getNodes(key2, keys...)
            : noNodes();
    }

    const node_list& getNodes(const boost::string_ref& key) const
    {
        auto&& child = nodes.get<referenced>().find(key);
        if (child != nodes.get<referenced>().end())
        {
            return child->value;
        }

        return noNodes();
    }

    const node_type& getParent() const
    {
        return parent;
    }

    void erase(const std::string& key)
    {
        nodes.get<referenced>().erase(key);
        values.get<referenced>().erase(key);
    }

    void eraseValue(const std::string& key, size_t index)
    {
        auto&& it = values.get<ordered>().find(key);
        if (it != values.get<ordered>().end())
        {
            values.get<ordered>().modify(it,
                [index](ElementType<value_list>& element)
                {
                    assert(element.value.size() > index);
                    element.value.erase(element.value.begin() + index);
                });
        }
    }

    void eraseNode(const std::string& key, size_t index)
    {
        auto&& it = nodes.get<ordered>().find(key);
        if (it != nodes.get<ordered>().end())
        {
            nodes.get<ordered>().modify(it,
                [index](ElementType<node_list>& element)
                {
                    assert(element.value.size() > index);
                    element.value.erase(element.value.begin() + index);
                });
        }
    }

    void overwrite(ConfigNode&& other);

    void swap(ConfigNode&& other)
    {
        nodes.swap(other.nodes);
        values.swap(other.values);
        updateParents(Recurse::no);
    }

    ConfigNode::node_type clone() const;

    template<typename V>
    void accept(V&& visitor)
    {
        for (auto&& child : nodes.get<sequenced>())
        {
            size_t index = 0;
            for (auto&& node : child.value)
            {
                visitor.visit(*this, child.key, index++, *node);
            }
        }
        for(auto&& child : values.get<sequenced>())
        {
            size_t index = 0;
            for (auto&& value : child.value)
            {
                //NOTE: const_cast is safe here as we are not touching the key
                visitor.visit(*this, child.key, index++, const_cast<std::string&>(value));
            }
        }
    }

private:
    template<typename S>
    void print(S& stream, const std::string& indent) const
    {
        for (const auto& child : nodes.get<sequenced>())
        {
            stream  << std::endl
                    << indent << child.key
                    << " ("   << std::hex << this << ")"
                    << " -> " << std::hex << parent.get();
            for (const auto& node : child.value)
            {
                node->print(stream, indent + "    ");
            }
        }
        for (const auto& child : values.get<sequenced>())
        {
            stream  << std::endl
                    << indent << child.key << " = [";
            for (const auto& value : child.value)
            {
                stream << value << ",";
            }
            stream << "] -> " << std::hex << parent.get();
        }
    }

    enum class Recurse { yes, no };
    void updateParents(Recurse recurse = Recurse::yes)
    {
        for(auto&& child : nodes)
        {
            for (auto&& node : child.value)
            {
                node->parent = shared_from_this();
                if (recurse == Recurse::yes)
                    node->updateParents();
            }
        }
    }

    static void clearJustSeparatorKey(const char*& key, Separator separator)
    {
        key = (key[0] != '\0' && key[1] == '\0' && key[0] == separator) ? "" : key;
    }

    static const value_list& noValues()
    {
        static const value_list empty;
        return empty;
    }

    static const node_list& noNodes()
    {
        static const node_list empty;
        return empty;
    }

    children_container<node_list> nodes;
    children_container<value_list> values;
    node_type parent;
};

} //namespace detail
} //namespace dconfig

