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
#include <memory>
#include <sstream>

#include "AttrTable.h" // libdap
#include "AggregationElement.h"
#include "AggregationUtil.h"
#include <Array.h> // libdap
#include <AttrTable.h>
#include "Dimension.h" // agg_util
#include "DimensionElement.h"
#include "Grid.h" // libdap
#include "GridAggregateOnOuterDimension.h"
#include "MyBaseTypeFactory.h"
#include "NCMLBaseArray.h"
#include "NCMLDebug.h"
#include "NCMLParser.h"
#include "NetcdfElement.h"
#include "ScanElement.h"

using agg_util::AggregationUtil;
using agg_util::AggMemberDataset;
using agg_util::GridAggregateOnOuterDimension;
using std::auto_ptr;

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
    , _scanners()
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
    , _scanners() // deep copy below
    , _aggVars(proto._aggVars)
  {
    // Deep copy all the datasets and add them to me...
    // This is potentially expensive in memory for large datasets, so let's tell someone.
    if (!proto._datasets.empty())
      {
        BESDEBUG("ncml",
            "WARNING: AggregationElement copy ctor is deep copying all contained datasets!  This might be memory and time intensive!");
      }

    // Clone the actual members
    _datasets.reserve(proto._datasets.size());
    for (vector<NetcdfElement*>::const_iterator it = proto._datasets.begin();
         it != proto._datasets.end();
         ++it)
      {
         const NetcdfElement* elt = (*it);
         addChildDataset(elt->clone());
      }
    NCML_ASSERT(_datasets.size() == proto._datasets.size());

    _scanners.reserve(proto._scanners.size());
    for (vector<ScanElement*>::const_iterator it = proto._scanners.begin();
         it != proto._scanners.end();
         ++it)
      {
        const ScanElement* elt = (*it);
        addScanElement(elt->clone());
      }
    NCML_ASSERT(_scanners.size() == proto._scanners.size());
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

    // And the scan elements
    while (!_scanners.empty())
      {
        ScanElement* elt = _scanners.back();
        _scanners.pop_back();
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
        BESDEBUG("ncml", "Added aggregation variable name=" + name << endl);
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

  void
  AggregationElement::addScanElement(ScanElement* pScanner)
  {
    VALID_PTR(pScanner);
    _scanners.push_back(pScanner);
    pScanner->ref(); // strong ref
    pScanner->setParent(this); // weak ref.
  }

  void
  AggregationElement::processParentDatasetComplete()
  {
    BESDEBUG("ncml", "AggregationElement::processParentDatasetComplete() called..." << endl);

    if (_type == "joinNew")
      {
        processParentDatasetCompleteForJoinNew();
      }
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
    // this will load ALL DDX's, but there's no choice for union.
    collectDatasetsInOrder(datasetsInOrder);
    DDS* pUnion = getParentDataset()->getDDS();
    AggregationUtil::performUnionAggregation(pUnion, datasetsInOrder);
  }

  // TODO Refactor as much of this as we can into AggregationUtil!!
  void
  AggregationElement::processJoinNew()
  {
    // This will run any child <scan> elements to prepare them.
    processAnyScanElements();

    BESDEBUG("ncml", "AggregationElement: beginning joinNew on the following aggVars=" +
        printAggregationVariables() << endl);

    // Union the dimensions of the chid sets so they're available
    BESDEBUG("ncml", "Merging dimensions from children into aggregated dataset..." << endl);
    mergeDimensions();

    // For now we will explicitly create the new dimension for lookups.
    unsigned int newDimSize = _datasets.size(); // ASSUMES we find an aggVar in EVERY dataset!
    getParentDataset()->addDimension(new DimensionElement(agg_util::Dimension(_dimName, newDimSize)));


    // We need at least one dataset, so warn.
    if (_datasets.empty())
      {
        THROW_NCML_PARSE_ERROR(line(), "In joinNew aggregation we cannot have zero datasets specified!");
      }

    // This is where the output variables go
    DDS* pAggDDS = getParentDataset()->getDDS();
    // The first dataset acts as the template for the remainder
    DDS* pTemplateDDS = _datasets[0]->getDDS();
    NCML_ASSERT_MSG(pTemplateDDS, "AggregationElement::processJoinNew(): NULL template dataset!");

    // First, union the template's global attribute table into the output's table.
    AggregationUtil::unionAttrTablesInto( &(pAggDDS->get_attr_table()),
                                          pTemplateDDS->get_attr_table() );

    // Then perform the aggregation for each variable...
    // TODO REFACTOR OPTIMIZE This is terrible!  We loop on variables,
    vector<string>::const_iterator endIt = _aggVars.end();
    for (vector<string>::const_iterator it = _aggVars.begin(); it != endIt; ++it)
      {
        const string& varName = *it;
        BESDEBUG("ncml", "Aggregating on aggVar=" << varName << "..." << endl);

        // Use the first dataset's variable as the template
        BaseType* pAggVarTemplate = NCMLUtil::getVariableNoRecurse(*pTemplateDDS, varName);
        if (!pAggVarTemplate)
          {
            THROW_NCML_PARSE_ERROR(line(),
                "In a joinNew aggregation, we could not find the aggregation variable=" +
                varName + " "
                "so we cannot continute the aggregation.");
          }
        if (pAggVarTemplate->type() == dods_array_c)
          {
            processAggVarJoinNewForArray(*pAggDDS, varName);
          }
        else if (pAggVarTemplate->type() == dods_grid_c)
          {
            processAggVarJoinNewForGrid(*pAggDDS, *(static_cast<Grid*>(pAggVarTemplate)) );
          }
        else
          {
            THROW_NCML_PARSE_ERROR(line(),
                "JoinNew Aggregation got an aggregation variable not of type Array or Grid, but of: " +
                pAggVarTemplate->type_name() + " which we cannot aggregate!");
          }
         // Nothing else to do for this var until the call to processParentDataset() is complete.
      }

    // Union any non-aggregated variables from the template dataset into the aggregated dataset
    // This will not override the aggregated, since the union will find they are already there
    // and skip them.
    AggregationUtil::unionAllVariablesInto(pAggDDS, *pTemplateDDS);

    // This requires all DDS to be loaded, which could take a long time.
    // We will DISALLOW it and only the first dataset's metadata and structure will
    // be used as a template.
    // Perform union on all unaggregated variables (i.e. not already added to parent)
    // AggregationUtil::performUnionAggregation(pAggDDS, datasetsInOrder);
  }

  void
  AggregationElement::processJoinExisting()
  {
    THROW_NCML_PARSE_ERROR(_parser->getParseLineNumber(),
        "Unimplemented aggregation type: joinExisting");

    // TODO make sure we warn of the slowness if scan is used
    // since every file needs to be opened until we make a caching daemon
    // as in the design doc...
  }

  /**
   * TODO OPTIMIZE The case for Array just loads everything!
   * We need to make it handle constraints properly as the
   * Grid path does.
   */
  void
  AggregationElement::processAggVarJoinNewForArray(DDS& aggDDS, const string& varName)
  {
    // If it's an Array type, not Grid:
    // For each aggregation variable:
    // 1) Look it up in the datasets in order and pull it out into an array
    // 2) Make a new NCMLArray<T> of the right type
    // 3) Make the AggregationUtil call to fill in the array
    // This will prepare the parent dataset with the aggregated variables.
    // This allows us to then just call perforUnionAggregation and it will
    // union the non agg ones in there since the aggregated ones already exist in union.

    vector<Array*> inputs;
    vector<DDS*> datasetsInOrder;
    // TODO OPTIMIZE This will load in every dataset's DDS regardless of constraints!!
    collectDatasetsInOrder(datasetsInOrder);
    inputs.reserve(datasetsInOrder.size());
    unsigned int found = AggregationUtil::collectVariableArraysInOrder(inputs, varName, datasetsInOrder);

    // We need one aggVar per set in order to set the dimension right.
    if (found != datasetsInOrder.size())
      {
        THROW_NCML_PARSE_ERROR(line(),
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
    auto_ptr<Array> newArrayBT = MyBaseTypeFactory::makeArrayTemplateVariable(arrayTypeName, varName, true);

    // Add the array to the parent dataset.  N.B. this copies it and adds the copy!
    aggDDS.add_var(newArrayBT.get());

    // So pull out the actual one in the DDS so we modify that one.
    Array* pJoinedArray = static_cast<Array*>(NCMLUtil::getVariableNoRecurse(aggDDS, varName));
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

  void
  AggregationElement::processAggVarJoinNewForGrid(DDS& aggDDS, const Grid& gridTemplate)
  {
    // Get the generic dimension object
    const DimensionElement* pDim = getParentDataset()->getDimensionInLocalScope(_dimName);
    NCML_ASSERT_MSG(pDim, "  AggregationElement::processAggVarJoinNewForGrid(): "
        " didn't find a DimensionElement with the joinNew dimName=" + _dimName );
    const agg_util::Dimension& dim = pDim->getDimension();

    // First, be sure the name isn't taken!  I had this bug before, seems we can add same
    // name twice!
    BaseType* pExists = NCMLUtil::getVariableNoRecurse(aggDDS, gridTemplate.name());
    NCML_ASSERT_MSG(!pExists, "processAggVarJoinNewForGrid failed since the name of"
        " the Grid to add (name=" + gridTemplate.name() + ") already exists in the "
        " output aggregation DDS!  What happened?!");

    vector<AggMemberDataset> memberDatasets;
    collectAggMemberDatasets(memberDatasets);

    // TODO Make sure this works with the scan elements as well.

    auto_ptr<GridAggregateOnOuterDimension> pAggGrid(new GridAggregateOnOuterDimension(
            gridTemplate,
            dim,
            memberDatasets,
            _parser->getDDSLoader()
            ));

    // This will copy, auto_ptr will clear the prototype.
    // TODO OPTIMIZE change to add_var_no_copy when it exists.
    BESDEBUG("ncml", "Adding new GridAggregateOnOuterDimension with name=" << gridTemplate.name() <<
        " to aggregated dataset!" << endl);
    aggDDS.add_var(pAggGrid.get());

    // We're done.  Just add it for now...
    // The processParentDatasetCompleteForJoinNew() will
    // Make sure the correct new map vector gets added
    // And if it's a data response we will have to factor out the constraints
    // and pass them down in the GridAggregateOuterDimension
  }

  void
  AggregationElement::processParentDatasetCompleteForJoinNew()
  {
    DDS* pParentDDS = getParentDataset()->getDDS();
    VALID_PTR(pParentDDS);

    const DimensionElement* pDim = getParentDataset()->getDimensionInLocalScope(_dimName);
    NCML_ASSERT_MSG(pDim, "  AggregationElement::processParentDatasetCompleteForJoinNew(): "
        " didn't find a DimensionElement with the joinNew dimName=" + _dimName );
    const agg_util::Dimension& dim = pDim->getDimension();

    // See if the author specified a valid coordinate variable.
    // We could throw a parse exception through here if the found
    // map var doesn't match our expectations...
    Array* pCV = findMatchingCoordinateVariable(*pParentDDS,
                                                dim, // this dimension
                                                true); // throw on invalid

    if (!pCV) // Name is free, so we need to create it
      {
        // Do the magic top-level call that figures out how to do it
        // Keep in auto_ptr since our add_var below will copy it.
        auto_ptr<libdap::Array> pNewCV = createCoordinateVariableForNewDimension(dim);

        // Make sure it did it
        NCML_ASSERT_MSG(pNewCV.get(), "AgregationElement::createCoordinateVariableForNewDimension() failed to create a coordinate variable!");

        // Add it to the DDS, which will make a copy
        // (TODO change this when we add noncopy add_var to DDS)
        pParentDDS->add_var(pNewCV.get());

        // Grab the copy back out and set to our expected result.
        pCV = static_cast<Array*>( NCMLUtil::getVariableNoRecurse(
                                        *(pParentDDS),
                                        dim.name) );

        NCML_ASSERT_MSG(pCV, "Logic Error: tried to add a new coordinate variable while processing joinNew"
            " but we couldn't locate it!");
      }

    // OK, either pCV is valid or we've unwound out by this point.
    // If a coordinate axis type was specified, we need to add it now.
    if (!_coordinateAxisType.empty())
      {
        addCoordinateAxisType(*pCV, _coordinateAxisType);
      }

    // For each aggVar:
    //    If it's a Grid, add the coordinate variable as a new map vector.
    //    If it's an Array, do nothing -- we already added the CV as a sibling to the aggvar
    AggVarIter it;
    AggVarIter endIt = endAggVarIter();
    for (it = beginAggVarIter(); it != endIt; ++it)
      {
        const string& aggVar = *it;
        BaseType* pBT = NCMLUtil::getVariableNoRecurse(*pParentDDS, aggVar);
        GridAggregateOnOuterDimension* pGrid = dynamic_cast<GridAggregateOnOuterDimension*>(pBT);
        if (pGrid)
          {
            // Add the given map to the Grid as a copy
            pGrid->prepend_map(pCV, true);
          }
      }
  }

  void
  AggregationElement::setAggregationVariableCoordinateAxisType(const std::string& cat)
  {
    _coordinateAxisType = cat;
  }

  const std::string&
  AggregationElement::getAggregationVariableCoordinateAxisType() const
  {
    return _coordinateAxisType;
  }

  libdap::Array*
  AggregationElement::findMatchingCoordinateVariable(
      const DDS& dds,
      const agg_util::Dimension& dim,
      bool throwOnInvalidCV/*=true*/) const
  {
    Array* pArrRet = 0;
    BaseType* pBT = NCMLUtil::getVariableNoRecurse(dds, dim.name);

    // Name doesn't exist, just NULL.  We'll have to create it.
    if (!pBT)
      {
        return 0;
      }

    // If 1D array with name == dim....
    if (AggregationUtil::couldBeCoordinateVariable(pBT))
      {
        // Ensure the dimensionalities match
        Array* pArr = static_cast<Array*>(pBT);
        if ( pArr->length() == static_cast<int>(dim.size) )
          {
            // OK, it's a valid return value.
            pArrRet = pArr;
          }
        else // Dimensionality mismatch, exception or return NULL.
          {
            ostringstream oss;
            oss << string("In joinNew aggregation for new dimension=")  << dim.name <<
                ": The coordinate variable we found does NOT have the same dimensionality as the"
                "joinNew dimension!  We expected dimensionality=" << dim.size <<
                " but the coordinate variable had dimensionality=" << pArr->length();
            BESDEBUG("ncml", oss.str() << endl);
            if (throwOnInvalidCV)
              {
                THROW_NCML_PARSE_ERROR(line(), oss.str());
              }
          }
      }

     else // Name exists, but not a coordinate variable, then exception or return null.
       {
         BESDEBUG("ncml",
             "AggregationElement::findMatchingCoordinateVariableAndThrowIfInvalid: "
             "Found a variable matching dimension name=" << dim.name <<
             " but it was not a coordinate variable" << endl);
         if (throwOnInvalidCV)
           {
             THROW_NCML_PARSE_ERROR(line(),
                 "AggregationElement::findMatchingCoordinateVariableAndThrowIfInvalid(): "
                 "We found a variable with the joinNew dimName=" + dim.name +
                 " but it is not a proper coordinate variable for the dimension.");
           }
       }
      // Return valid Array or null on failures.
      return pArrRet;
    }

  auto_ptr<libdap::Array>
  AggregationElement::createCoordinateVariableForNewDimension(const agg_util::Dimension& dim) const
  {
    // Get the netcdf@coordValue or use the netcdf@location (or autogenerate if empty() ).
    NCML_ASSERT(_datasets.size() > 0);
    bool hasCoordValue = !(_datasets[0]->coordValue().empty());
    if (hasCoordValue)
      {
        return createCoordinateVariableForNewDimensionUsingCoordValue(dim);
      }
    else
      {
        return createCoordinateVariableForNewDimensionUsingLocation(dim);
      }
  }

  auto_ptr<libdap::Array>
  AggregationElement::createCoordinateVariableForNewDimensionUsingCoordValue(const agg_util::Dimension& dim) const
  {
    NCML_ASSERT(_datasets.size() > 0);
    NCML_ASSERT_MSG(_datasets.size() == dim.size, "Logic error: Number of datasets doesn't match dimension!");
    // Use first dataset to define the proper type
    double doubleVal = 0;
    if (_datasets[0]->getCoordValueAsDouble(doubleVal))
      {
        return createCoordinateVariableForNewDimensionUsingCoordValueAsDouble(dim);
      }
    else
      {
        return createCoordinateVariableForNewDimensionUsingCoordValueAsString(dim);
      }
  }

  auto_ptr<libdap::Array>
  AggregationElement::createCoordinateVariableForNewDimensionUsingCoordValueAsDouble(const agg_util::Dimension& dim) const
  {
    vector<dods_float64> coords;
    coords.reserve(dim.size);
    double doubleVal = 0;
    // Use the index rather than iterator so we can use it in debug output...
    for (unsigned int i=0; i < _datasets.size(); ++i)
      {
        const NetcdfElement* pDataset = _datasets[i];
        if (!pDataset->getCoordValueAsDouble(doubleVal))
          {
            THROW_NCML_PARSE_ERROR(line(),
                "In creating joinNew coordinate variable from coordValue, expected a coordValue of type double"
                " but failed!  coordValue=" + pDataset->coordValue() +
                " which was in the dataset location=" + pDataset->location() +
                " with title=\"" + pDataset->title() + "\"");
          }
        else // we got our value fine, so add it
          {
            coords.push_back(static_cast<dods_float64>(doubleVal));
          }
      }

    // If we got here, we have the array of coords.
    // So we need to make the proper array, fill it in, and return it.
    auto_ptr<Array> pNewCV = MyBaseTypeFactory::makeArrayTemplateVariable("Array<Float64>", dim.name, true);
    NCML_ASSERT_MSG(pNewCV.get(), "createCoordinateVariableForNewDimensionUsingCoordValueAsDouble: failed to create"
        " the new Array<Float64> for variable: " + dim.name);
    pNewCV->append_dim(dim.size, dim.name);
    pNewCV->set_value(coords, coords.size()); // this will set the length correctly.
    return pNewCV;
  }

  auto_ptr<libdap::Array>
  AggregationElement::createCoordinateVariableForNewDimensionUsingCoordValueAsString(const agg_util::Dimension& dim) const
  {
    // I feel suitably dirty for cut and pasting this.
    vector<string> coords;
    coords.reserve(dim.size);
    for (unsigned int i=0; i < _datasets.size(); ++i)
      {
        const NetcdfElement* pDataset = _datasets[i];
        if (pDataset->coordValue().empty())
          {
            int parseLine = line();
            THROW_NCML_PARSE_ERROR(parseLine,
              "In creating joinNew coordinate variable from coordValue, expected a coordValue of type string"
              " but it was empty! dataset location=" + pDataset->location() +
              " with title=\"" + pDataset->title() + "\"");
          }
        else // we got our value fine, so add it
          {
            coords.push_back(pDataset->coordValue());
          }
      }
    // If we got here, we have the array of coords.
    // So we need to make the proper array, fill it in, and return it.
    auto_ptr<Array> pNewCV = MyBaseTypeFactory::makeArrayTemplateVariable("Array<String>", dim.name, true);
    NCML_ASSERT_MSG(pNewCV.get(), "createCoordinateVariableForNewDimensionUsingCoordValueAsString: failed to create"
        " the new Array<String> for variable: " + dim.name);
    pNewCV->append_dim(dim.size, dim.name);
    pNewCV->set_value(coords, coords.size()); // this will set the length correctly.
    return pNewCV;
  }

  auto_ptr<libdap::Array>
  AggregationElement::createCoordinateVariableForNewDimensionUsingLocation(const agg_util::Dimension& dim) const
  {
    // I feel suitably dirty for cut and pasting this.
    vector<string> coords;
    coords.reserve(dim.size);
    for (unsigned int i=0; i < _datasets.size(); ++i)
      {
        const NetcdfElement* pDataset = _datasets[i];
        string location("");
        if (pDataset->location().empty())
          {
            std::ostringstream oss;
            oss << "Virtual_Dataset_" << i;
            location = oss.str();
          }
        else // we got our value fine, so add it
          {
            location = pDataset->location();
          }
        coords.push_back(location);
      }
    // If we got here, we have the array of coords.
    // So we need to make the proper array, fill it in, and return it.
    auto_ptr<Array> pNewCV = MyBaseTypeFactory::makeArrayTemplateVariable("Array<String>", dim.name, true);
    NCML_ASSERT_MSG(pNewCV.get(), "createCoordinateVariableForNewDimensionUsingCoordValueUsingLocation: failed to create"
        " the new Array<String> for variable: " + dim.name);

    pNewCV->append_dim(dim.size, dim.name);
    pNewCV->set_value(coords, coords.size());
    return pNewCV;
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
  AggregationElement::collectAggMemberDatasets(vector<AggMemberDataset>& rMemberDatasets) const
  {
    rMemberDatasets.resize(0);
    rMemberDatasets.reserve(_datasets.size());

    for (vector<NetcdfElement*>::const_iterator it = _datasets.begin();
        it != _datasets.end();
        ++it)
      {
        const string& location = (*it)->location();
        rMemberDatasets.push_back( AggMemberDataset(location) );
      }
  }

  void
  AggregationElement::processAnyScanElements()
  {
    if (_scanners.size() > 0)
      {
        BESDEBUG("ncml", "Started to process " << _scanners.size() << " scan elements..." << endl);
      }

    vector<ScanElement*>::iterator it;
    vector<ScanElement*>::iterator endIt = _scanners.end();
    vector<NetcdfElement*> scannedDatasets;
    for (it = _scanners.begin(); it != endIt; ++it)
      {
         BESDEBUG("ncml", "Processing scan element = " << (*it)->toString() << " ..." << endl);

         // Run the scanner to get the scanned datasets.
         // These will be sorted, so maintain order.
         (*it)->getDatasetList(scannedDatasets);

         // Add the datasets using the parser call to
         // set the data up correctly,
         // then unref() and remove them from the temp array
         vector<NetcdfElement*>::iterator datasetIt;
         vector<NetcdfElement*>::iterator datasetEndIt = scannedDatasets.end();
         for (datasetIt = scannedDatasets.begin();
             datasetIt != datasetEndIt;
             ++datasetIt)
           {
             // this will ref() it and make sure we can load it.
             _parser->addChildDatasetToCurrentDataset(*datasetIt);
             // so we unref() it afterwards because we're dumping the temp array
             (*datasetIt)->unref();
           }
         // we're done with it and they're all unref().
         scannedDatasets.clear();
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

  static const string COORDINATE_AXIS_TYPE_ATTR("_CoordinateAxisType");
  void
  AggregationElement::addCoordinateAxisType(libdap::Array& rCV, const std::string& cat)
  {
    AttrTable& rAT = rCV.get_attr_table();
    AttrTable::Attr_iter foundIt = rAT.simple_find(COORDINATE_AXIS_TYPE_ATTR);
    // preexists, then delete it and we'll replace with the new
    if (foundIt != rAT.attr_end())
      {
        rAT.del_attr(COORDINATE_AXIS_TYPE_ATTR);
      }

    BESDEBUG("ncml", "Adding attribute to the aggregation variable " << rCV.name() <<
        " Attr is " << COORDINATE_AXIS_TYPE_ATTR <<
        " = " << cat <<
        endl);

    // Either way, now we can add it.
    rAT.append_attr(COORDINATE_AXIS_TYPE_ATTR, "String", cat);
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
