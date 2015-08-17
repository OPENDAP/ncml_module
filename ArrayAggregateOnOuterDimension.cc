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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//
// Please see the files COPYING and COPYRIGHT for more information on the GLPL.
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.
/////////////////////////////////////////////////////////////////////////////

#include "ArrayAggregateOnOuterDimension.h"
#include "AggregationException.h"

#include <DataDDS.h> // libdap::DataDDS
#include <Marshaller.h>
//#define DODS_DEBUG 1
//#include <debug.h>

// only NCML backlinks we want in this agg_util class.
#include "NCMLDebug.h" // BESDEBUG and throw macros
#include "NCMLUtil.h" // SAFE_DELETE, NCMLUtil::getVariableNoRecurse
#include "BESDebug.h"
#include "BESStopWatch.h"

// BES debug channel we output to
static const string DEBUG_CHANNEL("agg_util");

// Local flag for whether to print constraints, to help debugging
static const bool PRINT_CONSTRAINTS = false;

namespace agg_util {

ArrayAggregateOnOuterDimension::ArrayAggregateOnOuterDimension(const libdap::Array& proto,
    const AMDList& memberDatasets, std::auto_ptr<ArrayGetterInterface>& arrayGetter, const Dimension& newDim) :
    ArrayAggregationBase(proto, memberDatasets, arrayGetter) // no new dim yet in super chain
        , _newDim(newDim)
{
    BESDEBUG(DEBUG_CHANNEL, "ArrayAggregateOnOuterDimension: ctor called!" << endl);

    // Up the rank of the array using the new dimension as outer (prepend)
    BESDEBUG(DEBUG_CHANNEL, "ArrayAggregateOnOuterDimension: adding new outer dimension: " << _newDim.name << endl);
    prepend_dim(_newDim.size, _newDim.name);
}

ArrayAggregateOnOuterDimension::ArrayAggregateOnOuterDimension(const ArrayAggregateOnOuterDimension& proto) :
    ArrayAggregationBase(proto), _newDim()
{
    BESDEBUG(DEBUG_CHANNEL, "ArrayAggregateOnOuterDimension() copy ctor called!" << endl);
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
    if (this != &rhs) {
        cleanup();
        ArrayAggregationBase::operator=(rhs);
        duplicate(rhs);
    }
    return *this;
}

#define BUILD_ENTIRE_RESULT 0

// begin modifying here for the double buffering
bool ArrayAggregateOnOuterDimension::serialize(libdap::ConstraintEvaluator &eval, libdap::DDS &dds,
    libdap::Marshaller &m, bool ce_eval)
{
    BESStopWatch sw;
    if (BESISDEBUG(TIMING_LOG)) sw.start("ArrayAggregateOnOuterDimension::serialize", "");

    // Only continue if we are supposed to serialize this object at all.
    if (!(send_p() || is_in_selection())) {
        BESDEBUG_FUNC(DEBUG_CHANNEL, "Object not in output, skipping...  name=" << name() << endl);
        return true;
    }

    if (!read_p()) {
        BESDEBUG_FUNC(DEBUG_CHANNEL, " function entered..." << endl);

        if (PRINT_CONSTRAINTS) {
            BESDEBUG_FUNC(DEBUG_CHANNEL, "Constraints on this Array are:" << endl);
            printConstraints(*this);
        }

        // call subclass impl
        transferOutputConstraintsIntoGranuleTemplateHook();

        if (PRINT_CONSTRAINTS) {
            BESDEBUG_FUNC(DEBUG_CHANNEL, "After transfer, constraints on the member template Array are: " << endl);
            printConstraints(getGranuleTemplateArray());
        }

        // outer one is the first in iteration
        const Array::dimension& outerDim = *(dim_begin());
        BESDEBUG(DEBUG_CHANNEL,
            "Aggregating datasets array with outer dimension constraints: " << " start=" << outerDim.start << " stride=" << outerDim.stride << " stop=" << outerDim.stop << endl);

        // Be extra sure we have enough datasets for the given request
        if (static_cast<unsigned int>(outerDim.size) != getDatasetList().size()) {
            // Not sure whose fault it was, but tell the author
            THROW_NCML_PARSE_ERROR(-1, "The new outer dimension of the joinNew aggregation doesn't "
                " have the same size as the number of datasets in the aggregation!");
        }

#if BUILD_ENTIRE_RESULT
        // Prepare our output buffer for our constrained length
        reserve_value_capacity();
#else
        m.put_vector_start(length());
#endif
        // this index pointing into the value buffer for where to write.
        // The buffer has a stride equal to the _pSubArrayProto->length().

        // Keep this to do some error checking
        int nextElementIndex = 0;

        // Traverse the dataset array respecting hyperslab
        for (int i = outerDim.start; i <= outerDim.stop && i < outerDim.size; i += outerDim.stride) {
            AggMemberDataset& dataset = *((getDatasetList())[i]);

            try {
                dds.timeout_on();

                Array* pDatasetArray = AggregationUtil::readDatasetArrayDataForAggregation(getGranuleTemplateArray(),
                    name(), dataset, getArrayGetterInterface(), DEBUG_CHANNEL);

                dds.timeout_off();

#if BUILD_ENTIRE_RESULT
                this->set_value_slice_from_row_major_vector(*pDatasetArray, nextElementIndex);
#else
                m.put_vector_part(pDatasetArray->get_buf(), getGranuleTemplateArray().length(), var()->width(),
                    var()->type());
#endif
            }
            catch (agg_util::AggregationException& ex) {
                std::ostringstream oss;
                oss << "Got AggregationException while streaming dataset index=" << i << " data for location=\""
                    << dataset.getLocation() << "\" The error msg was: " << std::string(ex.what());
                THROW_NCML_PARSE_ERROR(-1, oss.str());
            }

            // Jump forward by the amount we added.
            nextElementIndex += getGranuleTemplateArray().length();
        }

        // If we succeeded, we are at the end of the array!
        NCML_ASSERT_MSG(nextElementIndex == length(), "Logic error:\n"
            "ArrayAggregateOnOuterDimension::read(): "
            "At end of aggregating, expected the nextElementIndex to be the length of the "
            "aggregated array, but it wasn't!");

#if !BUILD_ENTIRE_RESULT
        m.put_vector_end();
#endif

        // Set the cache bit to avoid recomputing
        set_read_p(true);
    }

#if BUILD_ENTIRE_RESULT
    libdap::Array::serialize(eval, dds, m, ce_eval);
#endif

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
// helpers

void ArrayAggregateOnOuterDimension::duplicate(const ArrayAggregateOnOuterDimension& rhs)
{
    _newDim = rhs._newDim;
}

void ArrayAggregateOnOuterDimension::cleanup() throw ()
{
}

/* virtual */
void ArrayAggregateOnOuterDimension::transferOutputConstraintsIntoGranuleTemplateHook()
{
    // transfer the constraints from this object into the subArray template
    // skipping our first dim which is the new one and not in the subArray.
    agg_util::AggregationUtil::transferArrayConstraints(&(getGranuleTemplateArray()), // into this template
        *this, // from this
        true, // skip first dim in the copy since we handle it special
        false, // also skip it in the toArray for the same reason.
        true, // print debug
        DEBUG_CHANNEL); // on this channel
}

/* virtual */
void ArrayAggregateOnOuterDimension::readConstrainedGranuleArraysAndAggregateDataHook()
{
    BESStopWatch sw;
    if (BESISDEBUG(TIMING_LOG))
        sw.start("ArrayAggregateOnOuterDimension::readConstrainedGranuleArraysAndAggregateDataHook", "");

    // outer one is the first in iteration
    const Array::dimension& outerDim = *(dim_begin());
    BESDEBUG(DEBUG_CHANNEL,
        "Aggregating datasets array with outer dimension constraints: " << " start=" << outerDim.start << " stride=" << outerDim.stride << " stop=" << outerDim.stop << endl);

    // Be extra sure we have enough datasets for the given request
    if (static_cast<unsigned int>(outerDim.size) != getDatasetList().size()) {
        // Not sure whose fault it was, but tell the author
        THROW_NCML_PARSE_ERROR(-1, "The new outer dimension of the joinNew aggregation doesn't "
            " have the same size as the number of datasets in the aggregation!");
    }

    // Prepare our output buffer for our constrained length
    reserve_value_capacity();

    // this index pointing into the value buffer for where to write.
    // The buffer has a stride equal to the _pSubArrayProto->length().
    int nextElementIndex = 0;

    // Traverse the dataset array respecting hyperslab
    for (int i = outerDim.start; i <= outerDim.stop && i < outerDim.size; i += outerDim.stride) {
        AggMemberDataset& dataset = *((getDatasetList())[i]);

        try {
#if 0
            agg_util::AggregationUtil::addDatasetArrayDataToAggregationOutputArray(*this, // into the output buffer of this object
                nextElementIndex, // into the next open slice
                getGranuleTemplateArray(), // constraints template
                name(), // aggvar name
                dataset, // Dataset who's DDS should be searched
                getArrayGetterInterface(), DEBUG_CHANNEL);
#endif
            Array* pDatasetArray = AggregationUtil::readDatasetArrayDataForAggregation(getGranuleTemplateArray(),
                name(), dataset, getArrayGetterInterface(), DEBUG_CHANNEL);

            this->set_value_slice_from_row_major_vector(*pDatasetArray, nextElementIndex);
        }
        catch (agg_util::AggregationException& ex) {
            std::ostringstream oss;
            oss << "Got AggregationException while streaming dataset index=" << i << " data for location=\""
                << dataset.getLocation() << "\" The error msg was: " << std::string(ex.what());
            THROW_NCML_PARSE_ERROR(-1, oss.str());
        }

        // Jump forward by the amount we added.
        nextElementIndex += getGranuleTemplateArray().length();
    }

    // If we succeeded, we are at the end of the array!
    NCML_ASSERT_MSG(nextElementIndex == length(), "Logic error:\n"
        "ArrayAggregateOnOuterDimension::read(): "
        "At end of aggregating, expected the nextElementIndex to be the length of the "
        "aggregated array, but it wasn't!");
}

}
