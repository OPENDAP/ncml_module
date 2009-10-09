//////////////////////////////////////////////////////////////////////////////
// This file is part of the "NcML Module" project, a BES module designed
// to allow NcML files to be used to be used as a wrapper to add
// AIS to existing datasets of any format.
//
// Copyright (c) 2009 OPeNDAP, Inc.
// Author: Michael Johnson  <m.johnson@opendap.org>
//
// For more information, please also see the main website: http://opendap.org/
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Please see the files COPYING and COPYRIGHT for more information on the GLPL.
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.
/////////////////////////////////////////////////////////////////////////////
#include "NCMLDebug.h"
#include "NCMLParser.h"
#include "OtherXMLParser.h"

namespace ncml_module
{

OtherXMLParser::OtherXMLParser(NCMLParser& p)
  : _rParser(p)
  , _depth(0)
  , _otherXML("")
{
}

OtherXMLParser::~OtherXMLParser()
{
  reset(); // for consistency
}

int
OtherXMLParser::getParseDepth() const
{
  return _depth;
}


const std::string&
OtherXMLParser::getString() const
{
  return _otherXML;
}

void
OtherXMLParser::reset()
{
  _depth = 0;
  _otherXML = "";
}

void
OtherXMLParser::onStartDocument()
{
  THROW_NCML_INTERNAL_ERROR("OtherXMLParser::onStartDocument called!  This is a logic bug.");
}

void
OtherXMLParser::onEndDocument()
{
  THROW_NCML_INTERNAL_ERROR("OtherXMLParser::onEndDocument called!  This is a logic bug.");
}

void
OtherXMLParser::onStartElement(const std::string& name, const AttributeMap& attrs)
{
  // Append this element and all its attributes onto the string
  _otherXML += "<" + name;

  // If there's attributes, copy them in...
  if (!attrs.empty())
    {
      for (AttributeMap::const_iterator it = attrs.begin();
          it != attrs.end();
          ++it)
        {
          _otherXML += ( string(" ") +
                        it->first +
                        "=\"" +
                        it->second +
                        "\"" );
        }
    }

  _otherXML += ">";

  ++_depth;
}

void
OtherXMLParser::onEndElement(const std::string& name)
{
  _otherXML += ( string("</") + name + ">" );
  --_depth;

  // Check for underflow
  if (_depth < 0)
    {
      // I am not sure this is internal or user can cause it... making it internal for now...
      THROW_NCML_INTERNAL_ERROR("OtherXMLElement::onEndElement: _depth < 0!  Logic error in parsing OtherXML.");
    }
}

void
OtherXMLParser::onCharacters(const std::string& content)
{
  // Really just shove them on there, whitespace and all to maintain formatting I guess.  Does this do that?
  _otherXML.append(content);
}

void
OtherXMLParser::onParseWarning(std::string msg)
{
  THROW_NCML_PARSE_ERROR(-1, // the SAX errors have the line in there already
      "OtherXMLParser: got SAX parse warning while parsing OtherXML.  Msg was: " + msg);
}

void
OtherXMLParser::onParseError(std::string msg)
{
  THROW_NCML_PARSE_ERROR(-1,
      "OtherXMLParser: got SAX parse error while parsing OtherXML.  Msg was: " + msg);
}

}
