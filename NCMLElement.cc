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
#include "AggregationElement.h"
#include "AttributeElement.h"
#include "DimensionElement.h"
#include "ExplicitElement.h"
#include "NetcdfElement.h"
#include "ReadMetadataElement.h"
#include "RemoveElement.h"
#include "ValuesElement.h"
#include "VariableElement.h"

using std::string;
using std::vector;

namespace ncml_module
{
  NCMLElement::Factory::Factory()
  : _protos()
  {
    initialize();
  }

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
  NCMLElement::Factory::initialize()
  {
    // Enter prototypes for all the concrete subclasses
    addPrototype(new RemoveElement());
    addPrototype(new ExplicitElement());
    addPrototype(new ReadMetadataElement());
    addPrototype(new NetcdfElement());
    addPrototype(new AttributeElement());
    addPrototype(new VariableElement());
    addPrototype(new ValuesElement());
    addPrototype(new DimensionElement());
    addPrototype(new AggregationElement());
  }


  RCPtr<NCMLElement>
  NCMLElement::Factory::makeElement(const string& eltTypeName, const AttributeMap& attrs)
  {
    ProtoList::const_iterator it = findPrototype(eltTypeName);
    if (it == _protos.end()) // not found
      {
        BESDEBUG("ncml", "NCMLElement::Factory cannot find prototype for element type=" << eltTypeName << endl);
        return RCPtr<NCMLElement>(0);
      }

    RCPtr<NCMLElement> newElt = RCPtr<NCMLElement>((*it)->clone());
    VALID_PTR(newElt.get());
    newElt->setAttributes(attrs);

    // Add the object to our pool to delete it when the factory dtor is called
    // if the ref count never hits 0.
    _pool.add(newElt.get());
    return newElt; //relinquish
  }

  bool
  NCMLElement::validateAttributes(const AttributeMap& attrs,
                                  const vector<string>& validAttrs,
                                  vector<string>* pInvalidAttrs /* = 0 */,
                                  bool printInvalid /* = true */,
                                  bool throwOnError /* = true */)
  {
    bool ret = true; // optimism in the face of uncertainty!

    // Make sure we always have an array to put results in.
    vector<string> myInvalidAttrs;
    if (!pInvalidAttrs)
      {
        pInvalidAttrs = &myInvalidAttrs;
      }
    VALID_PTR(pInvalidAttrs);

    // Check the lists
    if (!areAllAttributesValid(attrs, validAttrs, pInvalidAttrs))
      {
        // If any is wrong, cons up the list of errors.
        ret = false;

        // If we need to print or throw the error cases, then build the list up
        if (printInvalid || throwOnError)
          {
            std::ostringstream oss;
            oss << "Got invalid attribute for element = " << getTypeName();
            oss << " The invalid attributes were: {";
            for (unsigned int i=0; i<pInvalidAttrs->size(); ++i)
              {
                oss << (*pInvalidAttrs)[i];
                if (i < pInvalidAttrs->size()-1)  oss << ", ";
              }
            oss << "}";
            if (printInvalid)
              {
                BESDEBUG("ncml", oss.str() << endl);
              }
            if (throwOnError)
              {
                THROW_NCML_PARSE_ERROR(oss.str());
              }
          }
      }
    return ret;
  }

  std::string
  NCMLElement::printAttributeIfNotEmpty(const std::string& attrName, const std::string& attrValue)
  {
    return ( (attrValue.empty())?(""):(attrName + "=\"" + attrValue + "\" "));
  }

  bool
  NCMLElement::isValidAttribute(const std::vector<string>& validAttrs, const string& attr)
  {
    bool ret = false;
    for (unsigned int i=0; i<validAttrs.size(); ++i)
      {
        if (attr == validAttrs[i])
          {
            ret = true;
            break;
          }
      }
    return ret;
  }

  bool
  NCMLElement::areAllAttributesValid(const AttributeMap& attrMap, const std::vector<string>& validAttrs, std::vector<string>* pInvalidAttributes/*=0*/)
  {
    if (pInvalidAttributes)
      {
        pInvalidAttributes->resize(0);
      }
    bool ret = true;
    AttributeMap::const_iterator it;
    AttributeMap::const_iterator endIt = attrMap.end();
    for (it = attrMap.begin(); it != endIt; ++it)
      {
        const string& attr = it->first;
        if (!isValidAttribute(validAttrs, attr))
          {
            ret = false;
            if (pInvalidAttributes)
              {
                pInvalidAttributes->push_back(attr);
              }
            else
              {
                // Early exit only if we don't need the full list of bad ones...
                break;
              }
          }
      }
    return ret;
  }


} // namespace ncml_module
