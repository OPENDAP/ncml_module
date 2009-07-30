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
#ifndef __NCML_MODULE__NETCDF_ELEMENT_H__
#define __NCML_MODULE__NETCDF_ELEMENT_H__

#include "NCMLElement.h"

using namespace std;
namespace ncml_module
{

  /**
   * @brief Concrete class for NcML <netcdf> element
   *
   * This element specifies the location attribute for the local
   * data file that we wrap and load into a DDX (DDS w/ AttrTable tree).
   */
  class NetcdfElement : public NCMLElement
  {
  public:
    NetcdfElement();
    NetcdfElement(const NetcdfElement& proto);
    virtual ~NetcdfElement();
    virtual const string& getTypeName() const;
    virtual NetcdfElement* clone() const; // override clone with more specific subclass
    virtual void setAttributes(const AttributeMap& attrs);
    virtual void handleBegin(NCMLParser& p);
    virtual void handleContent(NCMLParser& p, const string& content);
    virtual void handleEnd(NCMLParser& p);
    virtual string toString() const;

    static const string _sTypeName;

  private:
    string _location;
  };

}

#endif /* __NCML_MODULE__NETCDF_ELEMENT_H__ */
