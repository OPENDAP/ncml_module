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

#include "NCMLCommonTypes.h"
#include "NCMLDebug.h"
#include "NCMLElement.h"

// Factory Includes for Concrete Subtypes
#include "AttributeElement.h"
#include "ExplicitElement.h"
#include "NetcdfElement.h"
#include "ReadMetadataElement.h"
#include "RemoveElement.h"
#include "VariableElement.h"

using std::string;
using std::vector;

namespace ncml_module
{
  // Start with no factory...
  NCMLElement::Factory* NCMLElement::Factory::_sInstance = 0;

  NCMLElement::Factory::~Factory()
  {
    // Delete all the prototype objects
    while (!_protos.empty())
      {
        const NCMLElement* proto = _protos.back();
        delete proto;
        _protos.pop_back();
      }
  }

  NCMLElement::Factory& NCMLElement::Factory::getTheFactory()
  {
    if (!_sInstance)
      {
        createTheFactory();
      }
    VALID_PTR(_sInstance);
    return *_sInstance;
  }

  void
  NCMLElement::Factory::addPrototype(const NCMLElement* proto)
  {
    VALID_PTR(proto);

    // If a proto already exists, erase and delete it so we can add the new one.
    const string& typeName = proto->getTypeName();
    ProtoList::iterator existingIt = findPrototype(typeName);
    if (existingIt != _protos.end())
      {
        BESDEBUG("ncml", "WARNING: Already got NCMLElement prototype for type=" << typeName << " so replacing with new one." << endl);
        const NCMLElement* oldOne = *existingIt;
        _protos.erase(existingIt);
        delete oldOne;
      }

    // Now it's safe to add new one
    _protos.push_back(proto);
  }

  NCMLElement::Factory::ProtoList::iterator
  NCMLElement::Factory::findPrototype(const std::string& elementTypeName)
  {
    ProtoList::iterator it = _protos.end();
    ProtoList::iterator endIt = _protos.end();
    for (it = _protos.begin(); it != endIt; ++it)
      {
        if ((*it)->getTypeName() == elementTypeName)
        {
          return it;
        }
      }
    return endIt;
  }

  void
  NCMLElement::Factory::createTheFactory()
  {
    _sInstance = new Factory();

    // Enter prototypes for all the concrete subclasses
    _sInstance->addPrototype(new RemoveElement());
    _sInstance->addPrototype(new ExplicitElement());
    _sInstance->addPrototype(new ReadMetadataElement());
    _sInstance->addPrototype(new NetcdfElement());
    _sInstance->addPrototype(new AttributeElement());
    _sInstance->addPrototype(new VariableElement());
  }


  std::auto_ptr<NCMLElement>
  NCMLElement::Factory::makeElement(const string& eltTypeName, const AttributeMap& attrs)
  {
    ProtoList::const_iterator it = findPrototype(eltTypeName);
    if (it == _protos.end()) // not found
      {
        BESDEBUG("ncml", "NCMLElement::Factory cannot find prototype for element type=" << eltTypeName << endl);
        return std::auto_ptr<NCMLElement>(0);
      }

    std::auto_ptr<NCMLElement> newElt = std::auto_ptr<NCMLElement>((*it)->clone());
    VALID_PTR(newElt.get());
    newElt->setAttributes(attrs);
    return newElt; //relinquish
  }

} // namespace ncml_module
