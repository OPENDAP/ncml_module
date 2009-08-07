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
#ifndef __NCML_MODULE__MYBASETYPEFACTORY_H__
#define __NCML_MODULE__MYBASETYPEFACTORY_H__

#include <memory>
#include <string>
#include "BaseType.h" // need the Type enum...

namespace libdap
{
  class BaseTypeFactory;
}

namespace ncml_module
{
  /**
   * @brief Wrapper for the BaseTypeFactory that lets us create by type name.
   *
   * The regular BaseTypeFactory doesn't have a factory method by type name,
   * so this wrapper will add the desired functionality.  It is a static class
   * rather than a subclass.
   */
  class MyBaseTypeFactory
  {
  protected: // static class for now
    MyBaseTypeFactory();
    virtual ~MyBaseTypeFactory();

  private: // disallow copies
    MyBaseTypeFactory(const MyBaseTypeFactory& rhs);
    MyBaseTypeFactory& operator=(const MyBaseTypeFactory& rhs);

  public:

    static libdap::Type getType(const string& name);

    /** @return whether the typeName refers to a simple (non-container) type. */
    static bool isSimpleType(const string& typeName);

    /** Return a new variable of the given type
     * @param type the DAP type
     * @param name the name to give the new variable
     * */
    static std::auto_ptr<libdap::BaseType> makeVariable(const libdap::Type& type, const std::string& name);

    /** Return a new variable of the given type name.  Return null if type is not valid.
     * @param type  the DAP type to create.
     * @param name the name to give the new variable
     * */
    static std::auto_ptr<libdap::BaseType> makeVariable(const string& type, const std::string& name);


  private: //data rep

    // Singleton factory we will use to create variables by type name
    static libdap::BaseTypeFactory* _spFactory;
  };

}

#endif /* __NCML_MODULE__MYBASETYPEFACTORY_H__ */
