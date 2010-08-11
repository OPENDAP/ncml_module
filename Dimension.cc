///////////////////////////////////////////////////////////////////////////////
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
#include "Dimension.h"
#include "NCMLDebug.h"// the only NCML dep we allow...
#include <sstream>

using std::string;
using std::ostringstream;
using std::vector;

namespace agg_util
{
  Dimension::Dimension()
  : name("")
  , size(0)
  , isShared(false)
  , isSizeConstant(false)
  {
  }

  Dimension::Dimension(const string& nameArg, unsigned int sizeArg, bool isSharedArg, bool isSizeConstantArg)
  : name(nameArg)
  , size(sizeArg)
  , isShared(isSharedArg)
  , isSizeConstant(isSizeConstantArg)
  {
  }

  Dimension::~Dimension()
  {
  }

  std::string
  Dimension::toString() const
  {
    ostringstream oss;
    oss << *this;
    return oss.str();
  }

  std::ostream& operator<<(std::ostream& os, const Dimension& dim)
  {
    os << "Dimension{";
    os << dim.name;
    os << "=";
    os << dim.size;
    os << "}";
    return os;
  }

  bool
  DimensionTable::findDimension(const std::string& name, Dimension* pOut) const
  {
    bool foundIt = false;
    vector<Dimension>::const_iterator endIt = _dimensions.end();
    vector<Dimension>::const_iterator it;
    for (it = _dimensions.begin(); it != endIt; ++it)
      {
        if (it->name == name)
          {
            if (pOut)
              {
                *pOut = *it;
              }
            foundIt = true;
            break;
          }
      }
    return foundIt;
  }

  void
  DimensionTable::addDimensionUnique(const Dimension& dim)
  {
    if (!findDimension(dim.name))
      {
        _dimensions.push_back(dim);
      }
    else
      {
        BESDEBUG("ncml", "A dimension with name=" << dim.name << " already exists.  Not adding." << endl);
      }
  }

  const std::vector<Dimension>&
  DimensionTable::getDimensions() const
  {
    return _dimensions;
  }

}
