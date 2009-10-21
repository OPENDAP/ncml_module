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
#include <Array.h> // libdap
#include <AttrTable.h>
#include "Dimension.h" // agg_util
#include "DimensionElement.h"
#include "MyBaseTypeFactory.h"
#include "NCMLBaseArray.h"
#include "NCMLDebug.h"
#include "NCMLParser.h"
#include "NetcdfElement.h"
#include <sstream>

using agg_util::AggregationUtil;

namespace ncml_module
{
  const string AggregationElement::_sTypeName = "aggregation";

  const vector<string> AggregationElement::_sValidAttrs = getValidAttributes();

  AggregationElement::AggregationElement()
    : NCMLElement(0)
    , _type("")
    , _dimName("")
    , _recheckEvery("")
    , _parent(0)
    , _datasets()
    , _aggVars()
  {
  }

  AggregationElement::AggregationElement(const AggregationElement& proto)
    : NCMLElement(proto)
    , _type(proto._type)
    , _dimName(proto._dimName)
    , _recheckEvery(proto._recheckEvery)
    , _parent(proto._parent) // my parent is the same too... is this safe without a true weak reference?
    , _datasets() // deep copy below
    , _aggVars(proto._aggVars)
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
    BESDEBUG("ncml:memory", "~AggregationElement called...");
    _type = "";
    _dimName= "";
    _recheckEvery = "";
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
  AggregationElement::setAttributes(const XMLAttributeMap& attrs)
  {
    _type = attrs.getValueForLocalNameOrDefault("type", "");
    _dimName = attrs.getValueForLocalNameOrDefault("dimName", "");
    _recheckEvery = attrs.getValueForLocalNameOrDefault("recheckEvery", "");

    // default is to print errors and throw which we want.
    validateAttributes(attrs, _sValidAttrs);
  }

  void
  AggregationElement::handleBegin()
  {
    NCML_ASSERT(!getParentDataset());

    // Check that the immediate parent element is netcdf since we cannot put an aggregation anywhere else.
    if (!_parser->isScopeNetcdf())
      {
        THROW_NCML_PARSE_ERROR(_parser->getParseLineNumber(),
          "Got an <aggregation> = " + toString() +
          " at incorrect parse location.  They can only be direct children of <netcdf>.  Scope=" +
          _parser->getScopeString());
      }

    NetcdfElement* dataset = _parser->getCurrentDataset();
    NCML_ASSERT_MSG(dataset, "We expected a non-noll current dataset while processing AggregationElement::handleBegin() for " + toString());
    // If the enclosing dataset already has an aggregation, this is a parse error.
    if (dataset->getChildAggregation())
      {
        THROW_NCML_PARSE_ERROR(_parser->getParseLineNumber(),
            "Got <aggregation> = " + toString() + " but the enclosing dataset = " + dataset->toString() +
            " already had an aggregation set!  There can be only one!");
      }
    // Set me as the aggregation for the current dataset.
    // This will set my parent and also ref() me.
    dataset->setChildAggregation(this);
  }

  void
  AggregationElement::handleContent(const string& content)
  {
    // Aggregations do not specify content!
    if (!NCMLUtil::isAllWhitespace(content))
      {
      THROW_NCML_PARSE_ERROR(_parser->getParseLineNumber(),
          "Got non-whitespace for content and didn't expect it.  Element=" + toString() + " content=\"" +
          content + "\"");
      }
  }

  void
  AggregationElement::handleEnd()
  {
    // Handle the actual processing!!
    BESDEBUG("ncml", "Got AggregationElement::handleEnd(): Processing the aggregation!!" << endl);

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
        THROW_NCML_PARSE_ERROR(_parser->getParseLineNumber(),
            "Sorry, we do not implement the forecastModelRunCollection aggregations in this version of the NCML Module!");
      }
    else
      {
        THROW_NCML_PARSE_ERROR(_parser->getParseLineNumber(),
            "Unknown aggregation type=" + _type + " at scope=" + _parser->getScopeString());
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

  void
  AggregationElement::addAggregationVariable(const string& name)
  {
    if (isAggregationVariable(name))
      {
        THROW_NCML_PARSE_ERROR(_parser->getParseLineNumber(),
            "Tried to add an aggregation variable twice: name=" + name +
            " at scope=" + _parser->getScopeString());
      }
    else
      {
        _aggVars.push_back(name);
        BESDEBUG("ncml", "Added aggregation variable name=" + name);
      }
  }

  bool
  AggregationElement::isAggregationVariable(const string& name) const
  {
    bool ret = false;
    AggVarIter endIt = endAggVarIter();
    AggVarIter it = beginAggVarIter();
    for (; it != endIt; ++it)
      {
        if (name == *it)
          {
            ret = true;
            break;
          }
      }
    return ret;
  }

  string
  AggregationElement::printAggregationVariables() const
  {
    string ret("{ ");
    AggVarIter endIt = endAggVarIter();
    AggVarIter it = beginAggVarIter();
    for (; it != endIt; ++it)
      {
        ret += *it;
        ret += " ";
      }
    ret += "}";
    return ret;
  }

  AggregationElement::AggVarIter
  AggregationElement::beginAggVarIter() const
  {
    return _aggVars.begin();
  }

  AggregationElement::AggVarIter
  AggregationElement::endAggVarIter() const
  {
    return _aggVars.end();
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

    // Merge all the dimensions...  For now, it is a parse error if a dimension
    // with the same name exists but has a different size.
    // Since DAP2 doesn't have dimensions, we can't do this in agg_util, but
    // have to do it here.
    mergeDimensions();

    // Merge the attributes and variables in all the DDS's into our parent DDS....
    vector<DDS*> datasetsInOrder;
    collectDatasetsInOrder(datasetsInOrder);
    DDS* pUnion = getParentDataset()->getDDS();
    AggregationUtil::performUnionAggregation(pUnion, datasetsInOrder);
  }

  void
  AggregationElement::processJoinNew()
  {
    BESDEBUG("ncml", "AggregationElement: beginning joinNew on the following aggVars=" +
        printAggregationVariables() << endl);

    // Union the dimensions of the chid sets so they're available
    BESDEBUG("ncml", "Merging dimensions from children into aggregated dataset..." << endl);
    mergeDimensions();

    // For each aggregation variable:
    // 1) Look it up in the datasets in order and pull it out into an array
    // 2) Make a new NCMLArray<T> of the right type
    // 3) Make the AggregationUtil call to fill in the array
    // This will prepare the parent dataset with the aggregated variables.
    // This allows us to then just call perforUnionAggregation and it will
    // union the non agg ones in there since the aggregated ones already exist in union.

    // Gather up the children datasets
    vector<DDS*> datasetsInOrder;
    collectDatasetsInOrder(datasetsInOrder);

    // This is where the output variables go
    DDS* pAggDDS = getParentDataset()->getDDS();

    // For now we will explicitly create the new dimension for lookups.
    unsigned int newDimSize = datasetsInOrder.size(); // ASSUMES we find an aggVar in EVERY dataset!
    getParentDataset()->addDimension(new DimensionElement(agg_util::Dimension(_dimName, newDimSize)));

    vector<string>::const_iterator endIt = _aggVars.end();
    for (vector<string>::const_iterator it = _aggVars.begin(); it != endIt; ++it)
      {
        const string& varName = *it;
        BESDEBUG("ncml", "Aggregating on aggVar=" << varName << "..." << endl);

        vector<Array*> inputs;
        inputs.reserve(datasetsInOrder.size());
        unsigned int found = AggregationUtil::collectVariableArraysInOrder(inputs, varName, datasetsInOrder);

        // We need one aggVar per set in order to set the dimension right.
        if (found != datasetsInOrder.size())
          {
            THROW_NCML_PARSE_ERROR(_parser->getParseLineNumber(),
                "In performing joinNew aggregation=" + toString() +
                " we did not find an aggregation variables named " + varName +
                " in all of the child datasets of the aggregation!");
          }

        // Create the output Array as NCMLArray of proper type,
        // saving in the auto_ptr since we'll work with a copy and want this destroyed
        BaseType* protoVar = inputs[0]->var();
        VALID_PTR(protoVar);
        const string& typeName = protoVar->type_name();
        string arrayTypeName = "Array<" + typeName + ">";
        auto_ptr<BaseType> newArrayBT = MyBaseTypeFactory::makeArrayTemplateVariable(arrayTypeName, varName);

        // Add the array to the parent dataset.  N.B. this copies it and adds the copy!
        pAggDDS->add_var(newArrayBT.get());

        // So pull out the actual one in the DDS so we modify that one.
        Array* pJoinedArray = dynamic_cast<Array*>(pAggDDS->var(varName));
        NCML_ASSERT_MSG(pJoinedArray, "Couldn't get the new aggregation Array from the parent DDS!");

        // Perform the aggregation, copying data as well if the parser is known to be doing a data request.
        bool handleData = _parser->parsingDataRequest();
        AggregationUtil::produceOuterDimensionJoinedArray(pJoinedArray, varName, _dimName, inputs, handleData);

        if (handleData)
          {
            // If it's data request, we need to make sure the NCMLArray<T> is properly finished up.
            NCMLBaseArray* pJoinedArrayDownCast = dynamic_cast<NCMLBaseArray*>(pJoinedArray);
            NCML_ASSERT_MSG(pJoinedArrayDownCast, "processJoinNew: dynamic cast to NCMLBaseArray for aggregation failed!");
            // Tell it to copy the values down and cache the current shape in case we get constraints added later!
            // TODO this is kind of smelly... Maybe we want a valueChanged() listener way up in Vector?
            // Seems like something useful to know in subclasses without them overriding all the set calls like I did here...
            pJoinedArrayDownCast->cacheSuperclassStateIfNeeded();
          }
      }

    // Perform union on all unaggregated variables (i.e. not already added to parent)
    AggregationUtil::performUnionAggregation(pAggDDS, datasetsInOrder);
  }

  void
  AggregationElement::processJoinExisting()
  {
    THROW_NCML_PARSE_ERROR(_parser->getParseLineNumber(),
        "Unimplemented aggregation type: joinExisting");
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

  void
  AggregationElement::mergeDimensions(bool checkDimensionMismatch/*=true*/)
  {
    NetcdfElement* pParent = getParentDataset();
    // For each dataset in the children....
    vector<NetcdfElement*>::const_iterator datasetsEndIt = _datasets.end();
    vector<NetcdfElement*>::const_iterator datasetsIt;
    for (datasetsIt = _datasets.begin(); datasetsIt != datasetsEndIt; ++datasetsIt)
      {
        // Check each dimension in it compared to the parent
        const NetcdfElement* dataset = *datasetsIt;
        VALID_PTR(dataset);
        const vector<DimensionElement*>& dimensions = dataset->getDimensionElements();
        vector<DimensionElement*>::const_iterator dimEndIt = dimensions.end();
        vector<DimensionElement*>::const_iterator dimIt;
        for (dimIt = dimensions.begin(); dimIt != dimEndIt; ++dimIt)
          {
            const DimensionElement* pDim = *dimIt;
            VALID_PTR(pDim);
            const DimensionElement* pUnionDim = pParent->getDimensionInLocalScope(pDim->name());
            if (pUnionDim)
              {
                // We'll check the dimensions match no matter what, but only warn unless we're told to check
                if (!pUnionDim->checkDimensionsMatch(*pDim))
                  {
                    string msg = string("The union aggregation already had a dimension=") +
                                        pUnionDim->toString() +
                                        " but we found another with different cardinality: " +
                                        pDim->toString() +
                                        " This is likely an error and could cause a later exception.";
                    BESDEBUG("ncml", "WARNING: " + msg);
                    if (checkDimensionMismatch)
                      {
                        THROW_NCML_PARSE_ERROR(_parser->getParseLineNumber(),
                            msg + " Scope=" + _parser->getScopeString());
                      }
                  }
              }
            else // if not in the union already, we want to add it!
              {
                // this will up the ref count for it so when child dataset dies, we're good.
                BESDEBUG("ncml", "Dimension name=" << pDim->name() <<
                    " was not found in the union yet, so adding it.  The full elt is: " <<
                    pDim->toString() << endl);
                pParent->addDimension( const_cast<DimensionElement*>(pDim) );
              }
          }
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
