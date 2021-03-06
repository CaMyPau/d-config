//          Copyright Adam Lach 2017
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

//local
#include <file_factory.hpp>
#include <default_builder.hpp>

//std
#include <fstream>

namespace dconfig {

Config FileFactory::create() const
{
    std::vector<std::string> contents;
    for(auto&& filename : files)
    {        
        std::ifstream in(filename, std::ios::in | std::ios::binary);
        if (in)
        {
            std::string loaded;
            in.seekg(0, std::ios::end);
            loaded.resize(in.tellg());
            in.seekg(0, std::ios::beg);
            in.read(&loaded[0], loaded.size());
            in.close();
            contents.push_back(std::move(loaded));
        }
    }
    return DefaultBuilder(separator).build(std::move(contents));
}

} //namespace dconfig

