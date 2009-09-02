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
#include "ExplicitElement.h"
#include "NCMLDebug.h"
#include "NCMLParser.h"
#include "NCMLUtil.h"
#include "NetcdfElement.h"

namespace ncml_module
{

  const string ExplicitElement::_sTypeName = "explicit";

  ExplicitElement::ExplicitElement()
  {
  }

  ExplicitElement::ExplicitElement(const ExplicitElement& /* proto */)
  : NCMLElement()
  {
  }

  ExplicitElement::~ExplicitElement()
  {
  }

  const string&
  ExplicitElement::getTypeName() const
  {
    return _sTypeName;
  }

  ExplicitElement*
  ExplicitElement::clone() const
  {
    return new ExplicitElement(*this);
  }

  void
  ExplicitElement::setAttributes(const AttributeMap& /* attrs */)
  {
  }

  void
  ExplicitElement::handleBegin(NCMLParser& p)
  {
    if (!p.isScopeNetcdf())
        {
          THROW_NCML_PARSE_ERROR("Got <explicit/> while not a direct child of a <netcdf>");
        }
    // this applies to the current dataset
    NetcdfElement* dataset = p.getCurrentDataset();
    VALID_PTR(dataset);

    if (dataset->getProcessedMetadataDirective())
      {
        THROW_NCML_PARSE_ERROR("Got " + toString() +
          " element but we already got a metadata directive for the current dataset!  Only one may be specified.");
      }

    dataset->setProcessedMetadataDirective();
    VALID_PTR(dataset->getDDS());
    p.clearAllAttrTables(dataset->getDDS());
  }

  void
  ExplicitElement::handleContent(NCMLParser& /* p */, const string& content)
  {
    if (!NCMLUtil::isAllWhitespace(content))
      {
        THROW_NCML_PARSE_ERROR("Got non-whitespace for element content and didn't expect it.  Element=" + toString() + " content=\"" +
            content + "\"");
      }
  }

  void
  ExplicitElement::handleEnd(NCMLParser& /* p */)
  {
  }

  string
  ExplicitElement::toString() const
  {
    return "<" + _sTypeName + ">";
  }
}
