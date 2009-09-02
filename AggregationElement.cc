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

#include "AggregationElement.h"
#include "AggregationUtil.h"
#include <AttrTable.h>
#include "NCMLDebug.h"
#include "NCMLParser.h"
#include "NetcdfElement.h"
#include <sstream>

namespace ncml_module
{
  const string AggregationElement::_sTypeName = "aggregation";

  const vector<string> AggregationElement::_sValidAttrs = getValidAttributes();

  AggregationElement::AggregationElement()
    : NCMLElement()
    , _type("")
    , _dimName("")
    , _recheckEvery("")
    , _parser(0)
    , _parent(0)
    , _datasets()
  {
  }

  AggregationElement::AggregationElement(const AggregationElement& proto)
    : NCMLElement(proto)
    , _type(proto._type)
    , _dimName(proto._dimName)
    , _recheckEvery(proto._recheckEvery)
    , _parser(proto._parser)
    , _parent(proto._parent) // my parent is the same too... is this safe without a true weak reference?
    , _datasets()
  {
    // Deep copy all the datasets and add them to me...
    // This is potentially expensive in memory for large datasets, so let's tell someone.
    if (!proto._datasets.empty())
      {
        BESDEBUG("ncml",
            "WARNING: AggregationElement copy ctor is deep copying all contained datasets!  This might be memory and time intensive!");
      }

    _datasets.reserve(proto._datasets.size());
    for (unsigned int i=0; i<proto._datasets.size(); ++i)
      {
         NetcdfElement* elt = proto._datasets[i];
         addChildDataset(elt->clone());
      }
    NCML_ASSERT(_datasets.size() == proto._datasets.size());
  }

  AggregationElement::~AggregationElement()
  {
    BESDEBUG("ncml", "~AggregationElement called...");
    _type = "";
    _dimName= "";
    _recheckEvery = "";
    _parser = 0;
    _parent = 0;

    // Release strong references to the contained netcdfelements....
    while (!_datasets.empty())
      {
        NetcdfElement* elt = _datasets.back();
        _datasets.pop_back();
        elt->unref(); // Will be deleted if the last strong reference
      }
  }

  const string&
  AggregationElement::getTypeName() const
  {
    return _sTypeName;
  }

  AggregationElement*
  AggregationElement::clone() const
  {
    return new AggregationElement(*this);
  }

  void
  AggregationElement::setAttributes(const AttributeMap& attrs)
  {
    _type = NCMLUtil::findAttrValue(attrs,"type", "");
    _dimName = NCMLUtil::findAttrValue(attrs, "dimName", "");
    _recheckEvery = NCMLUtil::findAttrValue(attrs, "recheckEvery", "");

    vector<string> invalidAttrs;
    if (!areAllAttributesValid(attrs, _sValidAttrs, &invalidAttrs))
      {
        std::ostringstream oss;
        oss << "Got invalid attribute for element = " << _sTypeName;
        oss << " The invalid attributes were: {";
        for (unsigned int i=0; i<invalidAttrs.size(); ++i)
          {
            oss << invalidAttrs[i];
            if (i < invalidAttrs.size()-1)  oss << ", ";
          }
        oss << "}";
        THROW_NCML_PARSE_ERROR(oss.str());
      }
  }

  void
  AggregationElement::handleBegin(NCMLParser& p)
  {
    NCML_ASSERT(!getParentDataset());

    // Check that the immediate parent element is netcdf since we cannot put an aggregation anywhere else.
    if (!p.isScopeNetcdf())
      {
        THROW_NCML_PARSE_ERROR("Got an <aggregation> = " + toString() +
          " at incorrect parse location.  They can only be direct children of <netcdf>.  Scope=" +
          p.getScopeString());
      }

    NetcdfElement* dataset = p.getCurrentDataset();
    NCML_ASSERT_MSG(dataset, "We expected a non-noll current dataset while processing AggregationElement::handleBegin() for " + toString());
    // If the enclosing dataset already has an aggregation, this is a parse error.
    if (dataset->getChildAggregation())
      {
        THROW_NCML_PARSE_ERROR("Got <aggregation> = " + toString() + " but the enclosing dataset = " + dataset->toString() +
            " already had an aggregation set!  There can be only one!");
      }
    // Set me as the aggregation for the current dataset.
    // This will set my parent and also ref() me.
    dataset->setChildAggregation(this);

    _parser = &p;
  }

  void
  AggregationElement::handleContent(NCMLParser& /* p */, const string& content)
  {
    // Aggregations do not specify content!
    if (!NCMLUtil::isAllWhitespace(content))
      {
      THROW_NCML_PARSE_ERROR("Got non-whitespace for content and didn't expect it.  Element=" + toString() + " content=\"" +
          content + "\"");
      }
  }

  void
  AggregationElement::handleEnd(NCMLParser& p)
  {
    // Handle the actual processing!!
    BESDEBUG("ncml", "Got AggregationElement::handleEnd(): Processing the aggregation!!");

    if (_type == "union")
      {
        processUnion();
      }
    else if (_type == "joinNew")
      {
        processJoinNew();
      }
    else if (_type == "joinExisting")
      {
        processJoinExisting();
      }
    else if (_type == "forecastModelRunCollection" ||
             _type == "forecastModelSingleRunCollection")
      {
        THROW_NCML_PARSE_ERROR("Sorry, we do not implement the forecastModelRunCollection aggregations in this version of the NCML Module!");
      }
    else
      {
        THROW_NCML_PARSE_ERROR("Unknown aggregation type=" + _type + " at scope=" + p.getScopeString());
      }

    _parser = 0;
  }

  string
  AggregationElement::toString() const
  {
    return  "<" + _sTypeName +
                " type=\"" + _type + "\"" +
                printAttributeIfNotEmpty("dimName", _dimName) +
                printAttributeIfNotEmpty("recheckEvery", _recheckEvery) +
            ">";
  }

  void
  AggregationElement::addChildDataset(NetcdfElement* pDataset)
  {
    VALID_PTR(pDataset);
    BESDEBUG("ncml", "AggregationElement: adding child dataset: " << pDataset->toString() << endl);

    // Add as a strong reference.
    pDataset->ref();
    _datasets.push_back(pDataset);

    // also set a weak reference to us as the parent
    pDataset->setParentAggregation(this);
  }


  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /// Private Impl

  NetcdfElement*
  AggregationElement::setParentDataset(NetcdfElement* parent)
  {
    NetcdfElement* ret = getParentDataset();
    _parent = parent;
    return ret;
  }

  void
  AggregationElement::processUnion()
  {
    BESDEBUG("ncml", "Processing a union aggregation..." << endl);
    vector<DDS*> datasetsInOrder;
    collectDatasetsInOrder(datasetsInOrder);
    // We will collect them INTO the parent DDS
    DDS* pUnion = getParentDataset()->getDDS();
    AggregationUtil::performUnionAggregation(pUnion, datasetsInOrder);
  }

  void
  AggregationElement::processJoinNew()
  {
    THROW_NCML_PARSE_ERROR("Unimplemented aggregation type: joinNew");
  }

  void
  AggregationElement::processJoinExisting()
  {
    THROW_NCML_PARSE_ERROR("Unimplemented aggregation type: joinExisting");
  }

  void
  AggregationElement::collectDatasetsInOrder(vector<DDS*>& ddsList) const
  {
    ddsList.resize(0);
    ddsList.reserve(_datasets.size());
    vector<NetcdfElement*>::const_iterator endIt = _datasets.end();
    vector<NetcdfElement*>::const_iterator it;
    for (it = _datasets.begin(); it != endIt; ++it)
      {
        const NetcdfElement* elt = *it;
        VALID_PTR(elt);
        DDS* pDDS = elt->getDDS();
        VALID_PTR(pDDS);
        ddsList.push_back(pDDS);
      }
  }


  vector<string>
  AggregationElement::getValidAttributes()
  {
    vector<string> attrs;
    attrs.push_back("type");
    attrs.push_back("dimName");
    attrs.push_back("recheckEvery");
    return attrs;
  }

}; // namespace ncml_module
