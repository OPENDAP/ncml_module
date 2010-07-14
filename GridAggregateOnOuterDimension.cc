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

#include "AggregationException.h"
#include "AggregationUtil.h" // agg_util
#include "Array.h" // libdap
#include "ArrayAggregateOnOuterDimension.h" // agg_util
#include "DataDDS.h" // libdap
#include "DDS.h" // libdap
#include "DDSLoader.h" // agg_util
#include "Dimension.h" // agg_util
#include "Grid.h" // libdap
#include <memory> // auto_ptr
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

  std::auto_ptr<Grid> pGridTemplateClone(( (rhs._pSubGridProto.get()) ?
      (static_cast<Grid*>(rhs._pSubGridProto->ptr_duplicate())) :
      (0) ));
  _pSubGridProto = pGridTemplateClone;
}

GridAggregateOnOuterDimension::GridAggregateOnOuterDimension(
    const Grid& proto,
    const Dimension& newDim,
    const AMDList& memberDatasets,
    const DDSLoader& loaderProto)
: Grid(proto) // this should give us map vectors and the member array rank (without new dim).
, _loader(loaderProto.getDHI()) // create a new loader with the given dhi... is this safely in scope?
, _newDim(newDim)
// make a clone of the Grid constraint template
, _pSubGridProto( static_cast<Grid*>(const_cast<Grid&>(proto).ptr_duplicate()))
{
  BESDEBUG("ncml:2", "GridAggregateOnOuterDimension() ctor called!" << endl);

  // nasty to throw in a ctor, but Grid
  // super will be fully created by here, so will dtor itself properly.
  createRep(memberDatasets);
}

GridAggregateOnOuterDimension::GridAggregateOnOuterDimension(const GridAggregateOnOuterDimension& proto)
: Grid(proto)
, _loader(proto._loader.getDHI())
, _newDim()
, _pSubGridProto(0)
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

const AMDList&
GridAggregateOnOuterDimension::getDatasetList() const
{
  ArrayAggregateOnOuterDimension* pAggArray =
      dynamic_cast<ArrayAggregateOnOuterDimension*>(
          const_cast<GridAggregateOnOuterDimension*>(this)->array_var());
  NCML_ASSERT_MSG(pAggArray, "GridAggregateOnOuterDimension::getDatasetList(): "
      "Expected an ArrayAggregateOnOuterDimension for the data array of "
      "the Grid, but failed to find it.");
  return pAggArray->getDatasetList();
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

  VALID_PTR(_pSubGridProto.get());

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
      pAggArray->read();
    }

  set_read_p(true);
  return true;
}

// Private

void
GridAggregateOnOuterDimension::createRep(const AMDList& memberDatasets)
{
  BESDEBUG("ncml:2", "GridAggregateOnOuterDimension: "
      "Replacing the Grid's data Array with an ArrayAggregateOnOuterDimension..." << endl);

  // This is the prototype we need.  It will have been set in the ctor.
  Array* pArr = static_cast<Array*>(array_var());
  NCML_ASSERT_MSG(pArr, "GridAggregateOnOuterDimension::createRep(): "
       "Expected to find a contained data Array but we did not!");

  // Create the Grid version of the read getter and make a new AAOOD from our state.
  std::auto_ptr<ArrayGetterInterface> arrayGetter(new TopLevelGridDataArrayGetter());

  // Create the subclass that does the work and replace our data array with it.
  // Note this ctor will prepend the new dimension itself, so we do not.
  std::auto_ptr<ArrayAggregateOnOuterDimension> aggDataArray(
        new ArrayAggregateOnOuterDimension(
            *pArr, // prototype, already should be setup properly _without_ the new dim
            _newDim,
            memberDatasets,
            _loader,
            arrayGetter));

  // Make sure null since sink function
  // called on the auto_ptr
  NCML_ASSERT(!(arrayGetter.get()));

  // Replace our data Array with this one.  Will delete old one and may throw.
  set_array(aggDataArray.get());

  // Release here on successful set since set_array uses raw ptr only.
  // In case we threw then auto_ptr cleans up itself.
  aggDataArray.release();
}

void
GridAggregateOnOuterDimension::cleanup() throw()
{
}

void
GridAggregateOnOuterDimension::readProtoSubGrid()
{
  VALID_PTR(_pSubGridProto.get());
  transferConstraintsToSubGrid(_pSubGridProto.get());

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
  VALID_PTR(_pSubGridProto.get());

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
      agg_util::AggregationUtil::transferArrayConstraints(subGridMap,
          *superGridMap,
          false, // skipFirstDim = false since map sizes consistent
          true, // printDebug
          "ncml:2"); // debugChannel
      ++subGridMapIt; // keep iterators in sync
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
  agg_util::AggregationUtil::transferArrayConstraints(
      pSubGridArray, // into the prototype
      *pThisArray, // from the output array (with valid constraints)
      true, // skipFirstDim: need to skip since the ranks differ
      true,  // printDebug
      "ncml:2" //debugChannel
      );
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
