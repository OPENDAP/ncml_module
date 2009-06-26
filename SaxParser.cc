#include "SaxParser.h"

using namespace ncml_module;

SaxParser::SaxParser()
{
}


const std::string&
SaxParser::findAttrValue(const SaxParser::AttrMap& map, const std::string& name, const std::string& def/*=""*/)
{
  AttrMap::const_iterator it = map.find(name);
  if (it == map.end())
    {
      return def;
    }
  else
    {
      return (*it).second;
    }
}
