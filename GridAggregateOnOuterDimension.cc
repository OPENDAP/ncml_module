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

#include "GridAggregateOnOuterDimension.h"

#include "AggregationUtil.h" // agg_util
#include "Array.h" // libdap
#include "DataDDS.h" // libdap
#include "DDS.h" // libdap
#include "DDSLoader.h" // agg_util
#include "Dimension.h" // agg_util
#include "Grid.h" // libdap
#include "NCMLDebug.h" // ncml_module
#include "NCMLUtil.h"  // ncml_module
#include <sstream>

using libdap::Array;
using libdap::DataDDS;
using libdap::DDS;
using libdap::Grid;

namespace agg_util
{

  // Local flag for whether to print constraints, to help debugging
  static const bool PRINT_CONSTRAINTS = true;

 // Copy local data
void
GridAggregateOnOuterDimension::duplicate(const GridAggregateOnOuterDimension& rhs)
{
  _loader = DDSLoader(rhs._loader.getDHI());
  _newDim = rhs._newDim;
  _datasetDescs = rhs._datasetDescs;
  _pSubGridProto = ( (rhs._pSubGridProto) ?
                    (static_cast<Grid*>(rhs._pSubGridProto->ptr_duplicate())) :
                    (0) );
}

GridAggregateOnOuterDimension::GridAggregateOnOuterDimension(
    const Grid& proto,
    const Dimension& newDim,
    const vector<AggMemberDataset>& memberDatasets,
    const DDSLoader& loaderProto)
: Grid(proto) // this should give us map vectors and the member array rank (without new dim).
, _loader(loaderProto.getDHI()) // create a new loader with the given dhi... is this safely in scope?
, _newDim(newDim)
, _datasetDescs(memberDatasets)
, _pSubGridProto(0)
{
  BESDEBUG("ncml:2", "GridAggregateOnOuterDimension() ctor called!" << endl);

  // Keep the Grid template around
  _pSubGridProto = static_cast<Grid*>(const_cast<Grid&>(proto).ptr_duplicate());

  // nasty to throw in a ctor, but Grid
  // super will be fully created by here, so will dtor itself properly.
  createRep();
}

GridAggregateOnOuterDimension::GridAggregateOnOuterDimension(const GridAggregateOnOuterDimension& proto)
: Grid(proto)
, _loader(proto._loader.getDHI())
, _newDim()
, _datasetDescs()
{
  BESDEBUG("ncml:2", "GridAggregateOnOuterDimension() copy ctor called!" << endl);
  duplicate(proto);
}

GridAggregateOnOuterDimension*
GridAggregateOnOuterDimension::ptr_duplicate()
{
  return new GridAggregateOnOuterDimension(*this);
}

GridAggregateOnOuterDimension&
GridAggregateOnOuterDimension::operator=(const GridAggregateOnOuterDimension& rhs)
{
  if (this == &rhs)
    {
      return *this;
    }

  Grid::_duplicate(rhs);
  cleanup();
  duplicate(rhs);
  return *this;
}

GridAggregateOnOuterDimension::~GridAggregateOnOuterDimension()
{
  BESDEBUG("ncml:2", "~GridAggregateOnOuterDimension() dtor called!" << endl);
  cleanup();
}

void
GridAggregateOnOuterDimension::set_send_p(bool state)
{
  BESDEBUG("ncml:2", "GridAggregateOnOuterDimension::set_send_p(" << ((state)?("true"):("false")) <<
      ") called!" << endl);
  Grid::set_send_p(state);
}

void
GridAggregateOnOuterDimension::set_in_selection(bool state)
{
  BESDEBUG("ncml:2", "GridAggregateOnOuterDimension::set_in_selection(" << ((state)?("true"):("false")) <<
      ") called!" << endl);
  Grid::set_in_selection(state);
}

bool
GridAggregateOnOuterDimension::read()
{
  BESDEBUG("ncml:2", "GridAggregateOnOuterDimension::read() called!" << endl);

  if (read_p())
    {
      BESDEBUG("ncml:2", "GridAggregateOnOuterDimension::read(): read_p() set, early exit!");
      return true;
    }

  if (PRINT_CONSTRAINTS)
    {
      printConstraints(*(get_array()));
    }

  VALID_PTR(_pSubGridProto);

  // Transfers constraints to the proto grid and reads it
  readProtoSubGrid();

  // Copy the read-in, constrained maps from the proto grid
  // into our output maps.
  copyProtoMapsIntoThisGrid();

  // Get the outer dimension constraints, which are the first ones
  Array* pAggArray = get_array();
  VALID_PTR(pAggArray);

  // Only do this portion if the array part is supposed to serialize!
  if (pAggArray->send_p() || pAggArray->is_in_selection())
    {
      readAndAggregateGrids();
    }

  set_read_p(true);
  return true;
}

void
GridAggregateOnOuterDimension::readAndAggregateGrids()
{
  Array* pAggArray = static_cast<Array*>(array_var());
  const Array::dimension& outerDim = *(pAggArray->dim_begin());
  BESDEBUG("ncml", "Aggregating datasets array with outer dimension constraints: " <<
      " start=" << outerDim.start <<
      " stride=" << outerDim.stride <<
      " stop=" << outerDim.stop << endl);

  // Be extra sure we have enough datasets for the given request
  if (outerDim.size != _datasetDescs.size())
    {
      // Not sure whose fault it was, but tell the author
      ostringstream oss;
      oss << "The new outer dimension of the joinNew aggregation doesn't " <<
            " have the same size as the number of datasets in the aggregation!";
      THROW_NCML_PARSE_ERROR(-1, oss.str());
    }

  // The prototype array is used to transfer constraints to
  // each dataset
  Array* pProtoArray = _pSubGridProto->get_array();
  VALID_PTR(pProtoArray);

  // Prepare the output array for the data to be streamed into it.
  pAggArray->reserve_value_capacity(); // for the constrained length()

  // this index pointing into the value buffer for where to write.
  // The buffer has a stride equal to the pProtoArray->length().
  int nextElementIndex = 0;

  // Traverse the dataset array respecting hyperslab
  // TODO OPTIMIZE this code can be optimized with
  // using pointers into the vector memory...
  for (int i = outerDim.start;
      i <= outerDim.stop && i < outerDim.size;
      i += outerDim.stride)
    {
      AggMemberDataset& dataset = _datasetDescs[i];

      // This call can throw on any kind of eerror
      addDatasetGridArrayDataToAggArray(pAggArray,
          nextElementIndex,
          *pProtoArray,
          name(),
          dataset);

      // Jump forward by the amount we added.
      nextElementIndex += pProtoArray->length();
    }

  // If we succeeded, we are at the end of the array!
  NCML_ASSERT_MSG(nextElementIndex == pAggArray->length(),
      "Logic error:\n"
        "GridAggregateOnOuterDimension::read(): "
      "At end of aggregating, expected the nextElementIndex to be the length of the "
      "aggregated array, but it wasn't!");
}

void
GridAggregateOnOuterDimension::createRep()
{
  // Add the new dimension to the data array.
  // We have created it from a prototype, so the Array should be there!
  Array* pArr = get_array();
  NCML_ASSERT_MSG(pArr, "GridAggregateOnOuterDimension::createRep(): "
      "Expected to find an Array but we did not!");
  BESDEBUG("ncml:2", "GridAggregateOnOuterDimension: adding new dimension to Array portion." << endl);
  // Up the rank of the array to the new dimension.
  pArr->prepend_dim(_newDim.size, _newDim.name);

  // TODO consider adding a placeholder map vector here with no data in it...
  BESDEBUG("ncml:2", "GridAggregateOnOuterDimension: NOT adding placeholder map: Consider adding this!" << endl);
}

void
GridAggregateOnOuterDimension::cleanup() throw()
{
  SAFE_DELETE(_pSubGridProto);
  _datasetDescs.clear(); // force the dtors on datasets to clear their memory
}

void
GridAggregateOnOuterDimension::addDatasetGridArrayDataToAggArray(
    Array* pAggArray,
    unsigned int atIndex,
    const Array& protoSubArray,
    const string& gridName,
    AggMemberDataset& dataset)
{
  dataset.loadDataDDS(_loader);
  DataDDS* pDataDDS = dataset.getDataDDS();
  NCML_ASSERT_MSG(pDataDDS, "GridAggregateOnOuterDimension::read(): Got a null DataDDS "
      "while loading dataset = " + dataset.getLocation() );

  // Grab the Grid in question
  BaseType* pDatasetBT = ncml_module::NCMLUtil::getVariableNoRecurse(*pDataDDS, gridName);
  if (!pDatasetBT)
    {
      // It better exist!
      THROW_NCML_PARSE_ERROR(-1,
          "GridAggregateOnOuterDimension::read(): Didn't find the aggregation variable with "
          "name=" + gridName +
          " in the aggregation member dataset location=" + dataset.getLocation() );
    }

  if (pDatasetBT->type() != libdap::dods_grid_c)
    {
      // It better be a Grid!
      THROW_NCML_PARSE_ERROR(-1,
          "GridAggregateOnOuterDimension::read(): The aggregation variable with "
          "name=" + name() + " in the aggregation member dataset location=" +
          dataset.getLocation() +
          " was NOT of type Grid!  Invalid aggregation!");
    }

  // Safe to cast and read it in now.
  Grid* pDatasetGrid = static_cast<Grid*>(pDatasetBT);

  // We only care about the array since the maps are irrelevant,
  // So transfer the constraints into it.
  Array* pDatasetArray = static_cast<Array*>(pDatasetGrid->array_var());
  NCML_ASSERT_MSG(pDatasetArray, "In aggregation member dataset, failed to get the array! "
        "Dataset location = " + dataset.getLocation());
  agg_util::AggregationUtil::transferArrayConstraints(pDatasetArray, protoSubArray, false, true, "ncml:2");

  // Force it to read...
  pDatasetGrid->set_send_p(true);
  pDatasetGrid->set_in_selection(true);
  pDatasetGrid->read();

  // Make sure that the data was read in or I dunno what went on.
  if (!pDatasetArray->read_p())
    {
      THROW_NCML_INTERNAL_ERROR("GridAggregateOnOuterDimension::addDatasetGridArrayDataToAggArray: the Grid's array pDatasetArray was not read_p()!");
    }

  // Make sure it matches the prototype or somthing went wrong
  if (!AggregationUtil::doTypesMatch(protoSubArray, *pDatasetArray))
    {
      THROW_NCML_PARSE_ERROR(-1,
        "Invalid aggregation! "
          "GridAggregateOnOuterDimension::read(): "
          "We found the Grid name=" + gridName +
          " but it was not of the same type as the prototype Grid!");
    }

  // Make sure the subshapes match! (true means check dimension names too... debate this)
  if (!AggregationUtil::doShapesMatch(protoSubArray, *pDatasetArray, true))
    {
      THROW_NCML_PARSE_ERROR(-1,
          "Invalid aggregation! "
          "GridAggregateOnOuterDimension::read(): "
          "We found the Grid name=" + gridName +
          " but it was not of the same shape as the prototype Grid!");
    }

  // OK, once we're here, make sure the length matches the proto
  if (protoSubArray.length() != pDatasetArray->length())
    {
      THROW_NCML_INTERNAL_ERROR("GridAggregateOnOuterDimension::addDatasetGridArrayDataToAggArray() "
          " The prototype array and the loaded dataset array length()'s were not equal, even "
          "though their shapes matched. Logic problem.");
    }

  // FINALLY, we get to stream the data!
  pAggArray->set_value_slice_from_row_major_vector(*pDatasetArray, atIndex);
}

void
GridAggregateOnOuterDimension::readProtoSubGrid()
{
  VALID_PTR(_pSubGridProto);
  transferConstraintsToSubGrid(_pSubGridProto);

  // Pass it the values for the aggregated grid...
  _pSubGridProto->set_send_p(send_p());
  _pSubGridProto->set_in_selection(is_in_selection());

  // Those settings will be used by read.
  _pSubGridProto->read();

  // For some reason, some handlers only set read_p for the parts, not the whole!!
  _pSubGridProto->set_read_p(true);
}

void
GridAggregateOnOuterDimension::copyProtoMapsIntoThisGrid()
{
  VALID_PTR(_pSubGridProto);

  Map_iter mapIt;
  Map_iter mapEndIt = map_end();
  for (mapIt = map_begin();
       mapIt != mapEndIt;
       ++mapIt)
    {
      Array* pOutMap = static_cast<Array*>(*mapIt);
      VALID_PTR(pOutMap);

      // If it isn't getting dumped, then don't bother with it
      if (! (pOutMap->send_p() || pOutMap->is_in_selection()) )
        {
          continue;
        }

      // If it's the new dimension, then it's already fine
      // Since it got it's constraints poked in directly
      // and contains its data.
      if (pOutMap->name() == _newDim.name)
        {
          if (PRINT_CONSTRAINTS)
            {
              BESDEBUG("ncml:2", "readMaps(): About to call read() on the map for the new outer dimension name=" <<
                _newDim.name <<
                " It's constraints are:" << endl);
              printConstraints(*pOutMap);
            }

          // Make sure it's read with these constraints.
          pOutMap->read();
          continue;
        }

      // Otherwise, find the map in the protogrid and copy it's data into this.
      Array* pProtoGridMap = findMapByName(*_pSubGridProto, pOutMap->name());
      NCML_ASSERT_MSG(pProtoGridMap, "GridAggregateOnOuterDimension::readMaps(): "
          "Couldn't find map in prototype grid for map name=" + pOutMap->name() );
      BESDEBUG("ncml:2",
          "readMaps(): About to call read() on prototype map vector name="
          << pOutMap->name() << " and calling transfer constraints..." << endl);

      // Make sure the protogrid maps were properly read
      NCML_ASSERT_MSG(pProtoGridMap->read_p(),
          "GridAggregateOnOuterDimension::readMaps(): "
          "Expected the prototype map to have been read but it wasn't.");

      // Make sure the lengths match to be sure we're not gonna blow memory up
      NCML_ASSERT_MSG(pOutMap->length() == pProtoGridMap->length(),
          "GridAggregateOnOuterDimension::readMaps(): "
          "Expected the prototype and output maps to have same length() "
          "after transfer of constraints, but they were not so we can't "
          "copy the data!");

      // The dimensions will have been set up correctly now so length() is correct...
      // We assume the _pSubGridProto matches at this point as well.
      // So we can use this call to copy from one vector to the other
      // so we don't use temp storage in between
      pOutMap->reserve_value_capacity(); // reserves mem for length
      pOutMap->set_value_slice_from_row_major_vector(*pProtoGridMap, 0);
      pOutMap->set_read_p(true);
    }

}

void
GridAggregateOnOuterDimension::transferConstraintsToSubGrid(Grid* pSubGrid)
{
  VALID_PTR(pSubGrid);
  transferConstraintsToSubGridMaps(pSubGrid);
  transferConstraintsToSubGridArray(pSubGrid);
}

void
GridAggregateOnOuterDimension::transferConstraintsToSubGridMaps(Grid* pSubGrid)
{
  BESDEBUG("ncml:2", "Transferring constraints to the subgrid maps..." << endl);
  Map_iter subGridMapIt = pSubGrid->map_begin();
  for (Map_iter it = map_begin(); it != map_end(); ++it)
    {
      // Skip the new outer dimension
      if (it == map_begin())
        {
          continue;
        }
      Array* subGridMap = static_cast<Array*>(*subGridMapIt);
      Array* superGridMap = static_cast<Array*>(*it);
      agg_util::AggregationUtil::transferArrayConstraints(subGridMap, *superGridMap, false, true, "ncml:2"); // the dims match, no skip
      ++subGridMapIt; // keep in sync
    }
}

void
GridAggregateOnOuterDimension::transferConstraintsToSubGridArray(Grid* pSubGrid)
{
  BESDEBUG("ncml:2", "Transferring constraints to the subgrid array..." << endl);

  Array* pSubGridArray = static_cast<Array*>(pSubGrid->get_array());
  VALID_PTR(pSubGridArray);
  Array* pThisArray = static_cast<Array*>(array_var());
  VALID_PTR(pThisArray);

  // transfer, skipping first dim which is the new one.
  agg_util::AggregationUtil::transferArrayConstraints(pSubGridArray, *pThisArray, true, true, "ncml:2");
}

void
GridAggregateOnOuterDimension::printConstraints(const Array& fromArray)
{
  ostringstream oss;
  AggregationUtil::printConstraints(oss, fromArray);
  BESDEBUG("ncml:2", "Constraints for Grid: " << name() << ": " << oss.str() << endl);
}

Array*
GridAggregateOnOuterDimension::findMapByName(Grid& inGrid, const string& findName)
{
  Map_iter it;
  Map_iter endIt = inGrid.map_end();
  for (it = inGrid.map_begin(); it != endIt; ++it)
    {
      if ( (*it)->name() == findName )
        {
          return static_cast<Array*>(*it);
        }
    }
  return 0;
}

} // namespace agg_util
