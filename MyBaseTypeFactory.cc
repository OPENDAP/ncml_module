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
#include "config.h"

#include "MyBaseTypeFactory.h"

#include "BaseType.h"
#include "BaseTypeFactory.h"

#include "Array.h"
#include "Byte.h"
#include "Float32.h"
#include "Float64.h"
#include "Grid.h"
#include "Int16.h"
#include "Int32.h"
#include "Sequence.h"
#include "Str.h"
#include "Structure.h"
#include "UInt16.h"
#include "UInt32.h"
#include "Url.h"

using namespace libdap;
using namespace std;

namespace ncml_module
{

  /* static */
  libdap::BaseTypeFactory* MyBaseTypeFactory::_spFactory = new BaseTypeFactory();

  MyBaseTypeFactory::MyBaseTypeFactory()
  {
  }

  MyBaseTypeFactory::~MyBaseTypeFactory()
  {
  }

  auto_ptr<libdap::BaseType>
  MyBaseTypeFactory::makeVariable(const libdap::Type& t, const string &name)
  {
      switch (t)
      {
        case dods_byte_c:
          return auto_ptr<BaseType>(_spFactory->NewByte(name));
          break;

        case dods_int16_c:
          return auto_ptr<BaseType>(_spFactory->NewInt16(name));
          break;

        case dods_uint16_c:
          return auto_ptr<BaseType>(_spFactory->NewUInt16(name));
          break;

        case dods_int32_c:
          return auto_ptr<BaseType>(_spFactory->NewInt32(name));
          break;

        case dods_uint32_c:
          return auto_ptr<BaseType>(_spFactory->NewUInt32(name));
          break;

        case dods_float32_c:
          return auto_ptr<BaseType>(_spFactory->NewFloat32(name));
          break;

        case dods_float64_c:
          return auto_ptr<BaseType>(_spFactory->NewFloat64(name));
          break;

        case dods_str_c:
          return auto_ptr<BaseType>(_spFactory->NewStr(name));
          break;

        case dods_url_c:
          return auto_ptr<BaseType>(_spFactory->NewUrl(name));
          break;

        case dods_array_c:
          return auto_ptr<BaseType>(_spFactory->NewArray(name));
          break;

        case dods_structure_c:
          return auto_ptr<BaseType>(_spFactory->NewStructure(name));
          break;

        case dods_sequence_c:
          return auto_ptr<BaseType>(_spFactory->NewSequence(name));
          break;

        case dods_grid_c:
          return auto_ptr<BaseType>(_spFactory->NewGrid(name));
          break;

        default:
          return auto_ptr<BaseType>(0);
      }
  }

  auto_ptr<libdap::BaseType>
  MyBaseTypeFactory::makeVariable(const string& type, const std::string& name)
  {
    return makeVariable(getType(type), name);
  }

  /** Get the Type enumeration value which matches the given name. */
  libdap::Type
  MyBaseTypeFactory::getType(const string& name)
  {
      if (name == "Byte")
          return dods_byte_c;

      if (name == "Int16")
          return dods_int16_c;

      if (name == "UInt16")
          return dods_uint16_c;

      if (name == "Int32")
          return dods_int32_c;

      if (name == "UInt32")
          return dods_uint32_c;

      if (name == "Float32")
          return dods_float32_c;

      if (name == "Float64")
          return dods_float64_c;

      if (name == "String" || name == "string")
          return dods_str_c;

      if (name == "URL")
          return dods_url_c;

      if (name == "Array")
          return dods_array_c;

      if (name == "Structure")
          return dods_structure_c;

      if (name == "Sequence")
          return dods_sequence_c;

      if (name == "Grid")
          return dods_grid_c;

      return dods_null_c;
  }

  bool
  MyBaseTypeFactory::isSimpleType(const string& name)
  {
      Type t = getType(name);
      switch (t) {
      case dods_byte_c:
      case dods_int16_c:
      case dods_uint16_c:
      case dods_int32_c:
      case dods_uint32_c:
      case dods_float32_c:
      case dods_float64_c:
      case dods_str_c:
      case dods_url_c:
          return true;
      default:
          return false;
      }
  }


}
