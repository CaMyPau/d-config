//          Copyright Adam Lach 2015
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

//local
#include <node.hpp>
#include <separator.hpp>

//boost
#include <boost/property_tree/ptree.hpp>
#include <boost/xpressive/xpressive.hpp>

//std
#include <cassert>
#include <iostream>

namespace dconfig {

class ConfigParamExpander
{
    struct RegexExpander
    {
        RegexExpander(const detail::Node* node, const Separator& aSeparator)
            : node(node)
            , separator(aSeparator)
        {
        }

        const std::string& operator() (const boost::xpressive::smatch& what) const
        {
            static const std::string empty;
            assert(what.size()>1);

            auto&& values = node->getValues((++what.begin())->str().c_str(), separator);
            if (!values.empty())
            {
                return values[0];
            }

            return empty;
        }

        const detail::Node* node;
        Separator separator;
    };

    struct Visitor
    {
        Visitor(detail::Node* root, const boost::xpressive::sregex* match, const Separator& aSeparator)
            : match(match)
            , root(root)
            , separator(aSeparator)
        {
        }

        void visit(detail::Node&, const std::string&, std::string& value)
        {
            using namespace boost::xpressive;
            value = regex_replace(value, *match, RegexExpander(root, separator));
        }

        void visit(detail::Node&, const std::string&, detail::Node& node)
        {
            node.accept(*this);
        }

        const boost::xpressive::sregex* match;
        detail::Node* root;
        Separator separator;
    };

public:
    explicit ConfigParamExpander(const Separator& aSeparator)
        : separator(aSeparator)
    {
    }

    void operator()(detail::Node& root)
    {
        using namespace boost::xpressive;

        sregex match = "%config." >> (s1 = -+_) >> "%";
        root.accept(Visitor(&root, &match, separator));
    }

private:
    Separator separator;
};

} //namespace dconfig

