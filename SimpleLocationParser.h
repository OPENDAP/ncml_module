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

#ifndef SIMPLELOCATIONPARSER_H_
#define SIMPLELOCATIONPARSER_H_

#include <string>
#include "SaxParser.h"

namespace ncml_module
{
  /**
   * @brief SaxParser implementation that just grabs the netcdf@location attribute and returns it.
   *
   * @see parseAndGetLocation()
   *
   * Currently will still process the entire file, but the callbacks are inlines so should
   * be fast enough.  Not sure if we can stop the libxml parser midstream yet.
   *
   * @author Michael Johnson <m.johnson@opendap.org>
   */
  class SimpleLocationParser : public SaxParser
  {
  private:
    std::string _location;

  public:
    SimpleLocationParser();
    virtual ~SimpleLocationParser();

    /**
     *  Parse the NcML filename and return the netcdf@location attribute,
     *  assuming there's only one netCDF node.  If more than one,
     *  we'll get the last node's location.
     */
    std::string parseAndGetLocation(const std::string& filename);

    /////////////// Interface SaxParser
    virtual void onStartDocument() {  }
    virtual void onEndDocument() {  }

    /** We only use this get the the nedcdf@location attribute out */
    virtual void onStartElement(const std::string& name, const AttrMap& attrs);

    virtual void onEndElement(const std::string& /* name */) {  }
    virtual void onCharacters(const std::string& /* content */) {  };

    virtual void onParseWarning(std::string msg);
    virtual void onParseError(std::string msg);
  };

}

#endif /* SIMPLELOCATIONPARSER_H_ */
