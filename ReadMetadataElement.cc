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
#include "ReadMetadataElement.h"
#include "NCMLDebug.h"
#include "NCMLParser.h"
#include "NCMLUtil.h"

namespace ncml_module
{

  const string ReadMetadataElement::_sTypeName = "readMetadata";

  ReadMetadataElement::ReadMetadataElement()
  {
  }

  ReadMetadataElement::ReadMetadataElement(const ReadMetadataElement& /* proto */)
  : NCMLElement()
  {
  }

  ReadMetadataElement::~ReadMetadataElement()
  {
  }

  const string&
  ReadMetadataElement::getTypeName() const
  {
    return _sTypeName;
  }

  ReadMetadataElement*
  ReadMetadataElement::clone() const
  {
    return new ReadMetadataElement(*this);
  }

  void
  ReadMetadataElement::setAttributes(const AttributeMap& /* attrs */)
  {
  }

  void
  ReadMetadataElement::handleBegin(NCMLParser& p)
  {
    if (!p.withinNetcdf())
        {
          THROW_NCML_PARSE_ERROR("Got <readMetadata/> while not within <netcdf>");
        }
      p.changeMetadataDirective(NCMLParser::READ_METADATA);
  }

  void
  ReadMetadataElement::handleContent(NCMLParser& /* p */, const string& content)
  {
    if (!NCMLUtil::isAllWhitespace(content))
      {
        THROW_NCML_PARSE_ERROR("Got non-whitespace for element content and didn't expect it.  Element=" + toString() + " content=\"" +
            content + "\"");
      }
  }

  void
  ReadMetadataElement::handleEnd(NCMLParser& /* p */)
  {
  }

  string
  ReadMetadataElement::toString() const
  {
    return "<" + _sTypeName + ">";
  }
}
