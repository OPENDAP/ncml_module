//////////////////////////////////////////////////////////////////////////////
// This file is part of the "NcML Module" project, a BES module designed
// to allow NcML files to be used to be used as a wrapper to add
// AIS to existing datasets of any format.
//
// Copyright (c) 2010 OPeNDAP, Inc.
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

#include "ArrayAggregateOnOuterDimension.h"

#include "AggregationException.h"
#include <DataDDS.h> // libdap::DataDDS

// only NCML backlinks we want in this agg_util class.
#include "NCMLDebug.h" // BESDEBUG and throw macros
#include "NCMLUtil.h" // SAFE_DELETE, NCMLUtil::getVariableNoRecurse

// BES debug channel we output to
static const string DEBUG_CHANNEL("ncml:2");

// Local flag for whether to print constraints, to help debugging
static const bool PRINT_CONSTRAINTS = true;

namespace agg_util
{

  ArrayAggregateOnOuterDimension::ArrayAggregateOnOuterDimension(
      const libdap::Array& proto,
      const Dimension& newDim,
      const AMDList& memberDatasets,
      const DDSLoader& loaderProto,
      std::auto_ptr<ArrayGetterInterface>& arrayGetter)
  : Array(proto) // ctor on super from the proto, will not have the new dim
  , _loader(loaderProto.getDHI()) // copy from DataHandlerInterface
  , _newDim(newDim)
  , _datasetDescs(memberDatasets) // copy the vector of RCPtr will ref()
  , _pSubArrayProto(static_cast<Array*>(const_cast<Array&>(proto).ptr_duplicate()))
  , _pArrayGetter(arrayGetter)
  {
    BESDEBUG(DEBUG_CHANNEL, "ArrayAggregateOnOuterDimension: ctor called!" << endl);

    // Up the rank of the array using the new dimension as outer (prepend)
    BESDEBUG(DEBUG_CHANNEL,
        "ArrayAggregateOnOuterDimension: adding new outer dimension: "
        << _newDim.name
        << endl);
    prepend_dim(_newDim.size, _newDim.name);
  }

  ArrayAggregateOnOuterDimension::ArrayAggregateOnOuterDimension(
      const ArrayAggregateOnOuterDimension& proto
      )
  : Array(proto)
  , _loader(proto._loader.getDHI())
  , _newDim()
  , _datasetDescs() // copied below
  , _pSubArrayProto(0) // duplicate() handles this
  , _pArrayGetter(0) // duplicate() handles this
  {
    BESDEBUG(DEBUG_CHANNEL,
        "ArrayAggregateOnOuterDimension() copy ctor called!" <<
        endl);
    duplicate(proto);
  }

  ArrayAggregateOnOuterDimension::~ArrayAggregateOnOuterDimension()
  {
    BESDEBUG(DEBUG_CHANNEL, "~ArrayAggregateOnOuterDimension() dtor called!" << endl);
    cleanup();
  }

  ArrayAggregateOnOuterDimension*
  ArrayAggregateOnOuterDimension::ptr_duplicate()
  {
    return new ArrayAggregateOnOuterDimension(*this);
  }

  ArrayAggregateOnOuterDimension&
  ArrayAggregateOnOuterDimension::operator=(const ArrayAggregateOnOuterDimension& rhs)
  {
    if (this == &rhs)
      {
        return *this;
      }

    // Copy super
    Array::_duplicate(rhs);

    // Clean out and copy this subclass's local data
    cleanup();
    duplicate(rhs);

    return *this;
  }

  void
  ArrayAggregateOnOuterDimension::set_send_p(bool state)
  {
    BESDEBUG(DEBUG_CHANNEL, "ArrayAggregateOnOuterDimension::set_send_p(" << ((state)?("true"):("false")) <<
                ") called!" << endl);
    Array::set_send_p(state);
  }

  void
  ArrayAggregateOnOuterDimension::set_in_selection(bool state)
  {
    BESDEBUG(DEBUG_CHANNEL, "ArrayAggregateOnOuterDimension::set_in_selection(" << ((state)?("true"):("false")) <<
                ") called!" << endl);
    Array::set_in_selection(state);
  }

  const AMDList&
  ArrayAggregateOnOuterDimension::getDatasetList() const
  {
    return _datasetDescs;
  }

  bool
  ArrayAggregateOnOuterDimension::read()
  {
    BESDEBUG(DEBUG_CHANNEL,"ArrayAggregateOnOuterDimension::read() called! " << endl);

    // Early exit if already done, avoid doing it twice!
    if (read_p())
      {
        BESDEBUG(DEBUG_CHANNEL, "ArrayAggregateOnOuterDimension::read(): read_p() set, early exit!");
        return true;
      }

    // Only continue if we are supposed to serialize this object at all.
    if (! (send_p() || is_in_selection()) )
      {
        BESDEBUG(DEBUG_CHANNEL, "ArrayAggregateOnOuterDimension::read(): "
            "Object not in output, skipping...  name=" <<
            name() <<
            endl);
        return true;
      }

    if (PRINT_CONSTRAINTS)
      {
        BESDEBUG(DEBUG_CHANNEL, "ArrayAggregateOnOuterDimension::read():"
            "Constraints on this Array are:" <<
            endl);
        printConstraints(*this);
      }

    // transfer the constraints from this object into the subArray template
    // skipping our first dim which is the new one and not in the subArray.
    agg_util::AggregationUtil::transferArrayConstraints(
        _pSubArrayProto.get(), // into template
        *this, // from this
        true, // skip first dim in the copy since one is a sub.
        true, // print debug
        DEBUG_CHANNEL); // on this channel

    if (PRINT_CONSTRAINTS)
      {
        BESDEBUG(DEBUG_CHANNEL, "ArrayAggregateOnOuterDimension::read():"
            "After transfer, constraints on the member template Array are: " <<
            endl);
        printConstraints(*_pSubArrayProto);
      }

    // Read in the datasets in the constrained output and stream them into
    // the output buffer of this.
    readMemberArraysAndAggregateData();

    // Set the cache bit
    set_read_p(true);

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////////////////
  // helpers

  void
  ArrayAggregateOnOuterDimension::duplicate(const ArrayAggregateOnOuterDimension& rhs)
  {
    _loader = DDSLoader(rhs._loader.getDHI());
    _newDim = rhs._newDim;

    NCML_ASSERT(_datasetDescs.empty());
    // Make a copy of the vector and all elements
    _datasetDescs = rhs._datasetDescs;

    // Clone the template if it isn't null.
    std::auto_ptr<Array> pTemplateClone( ( (rhs._pSubArrayProto.get()) ?
        (static_cast<Array*>(rhs._pSubArrayProto->ptr_duplicate())) :
        (0) ));
    _pSubArrayProto = pTemplateClone;

    // Clone the ArrayGetterInterface as well.
    std::auto_ptr<ArrayGetterInterface> pGetterClone(
        (rhs._pArrayGetter.get()) ?
        ( rhs._pArrayGetter->clone() ) :
        (0) );
    _pArrayGetter = pGetterClone;
  }

  void
  ArrayAggregateOnOuterDimension::cleanup() throw()
  {
    _datasetDescs.clear();
    _datasetDescs.resize(0);
  }

  void
  ArrayAggregateOnOuterDimension::printConstraints(const Array& fromArray)
  {
    ostringstream oss;
    AggregationUtil::printConstraints(oss, fromArray);
    BESDEBUG(DEBUG_CHANNEL, "Constraints for Array: " << name() << ": " << oss.str() << endl);
  }

  void
  ArrayAggregateOnOuterDimension::readMemberArraysAndAggregateData()
  {
     // outer one is the first in iteration
     const Array::dimension& outerDim = *(dim_begin());
     BESDEBUG("ncml", "Aggregating datasets array with outer dimension constraints: " <<
         " start=" << outerDim.start <<
         " stride=" << outerDim.stride <<
         " stop=" << outerDim.stop << endl);

     // Be extra sure we have enough datasets for the given request
     if (static_cast<unsigned int>(outerDim.size) != _datasetDescs.size())
       {
         // Not sure whose fault it was, but tell the author
         ostringstream oss;
         oss << "The new outer dimension of the joinNew aggregation doesn't " <<
               " have the same size as the number of datasets in the aggregation!";
         THROW_NCML_PARSE_ERROR(-1, oss.str());
       }

     // Prepare our output buffer for our constrained length
     reserve_value_capacity();

     // this index pointing into the value buffer for where to write.
     // The buffer has a stride equal to the _pSubArrayProto->length().
     int nextElementIndex = 0;

     // Traverse the dataset array respecting hyperslab
     for (int i = outerDim.start;
         i <= outerDim.stop && i < outerDim.size;
         i += outerDim.stride)
       {
         AggMemberDataset& dataset = *(_datasetDescs[i]);

         try
         {
           agg_util::AggregationUtil::addDatasetArrayDataToAggregationOutputArray(
               *this, // into the output buffer of this object
               nextElementIndex, // into the next open slice
               *_pSubArrayProto, // constraints template
               name(), // aggvar name
               dataset, // Dataset who's DDS should be searched
               *_pArrayGetter,
               "ncml:2"
           );
         }
         catch (agg_util::AggregationException& ex)
         {
           std::ostringstream oss;
           oss << "Got AggregationException while streaming dataset index=" << i <<
               " data for location=\"" << dataset.getLocation() <<
               "\" The error msg was: " << std::string(ex.what());
           THROW_NCML_PARSE_ERROR(-1, oss.str());
         }

         // Jump forward by the amount we added.
         nextElementIndex += _pSubArrayProto->length();
       }

     // If we succeeded, we are at the end of the array!
     NCML_ASSERT_MSG(nextElementIndex == length(),
         "Logic error:\n"
           "ArrayAggregateOnOuterDimension::read(): "
         "At end of aggregating, expected the nextElementIndex to be the length of the "
         "aggregated array, but it wasn't!");
  }

}
