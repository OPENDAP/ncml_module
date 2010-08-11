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
#include "AggMemberDatasetWithDimensionCacheBase.h"

#include "AggregationException.h" // agg_util
#include "Array.h" // libdap
#include "BaseType.h" // libdap
#include "Constructor.h" // libdap
#include "DataDDS.h" // libdap
#include "DDS.h" // libdap
#include "NCMLDebug.h"
#include <sstream>

using std::string;
using libdap::BaseType;
using libdap::Constructor;
using libdap::DataDDS;
using libdap::DDS;

static const string DEBUG_CHANNEL("agg_util");

namespace agg_util
{

  // Used to init the DimensionCache below with an estimated number of dimensions
  static const unsigned int DIMENSION_CACHE_INITIAL_SIZE = 4;

  AggMemberDatasetWithDimensionCacheBase::AggMemberDatasetWithDimensionCacheBase(
      const std::string& location)
  : AggMemberDataset(location)
  , _dimensionCache(DIMENSION_CACHE_INITIAL_SIZE)
  {
  }

  /* virtual */
  AggMemberDatasetWithDimensionCacheBase::~AggMemberDatasetWithDimensionCacheBase()
  {
    _dimensionCache.clear();
    _dimensionCache.resize(0);
  }

  AggMemberDatasetWithDimensionCacheBase::AggMemberDatasetWithDimensionCacheBase(
          const AggMemberDatasetWithDimensionCacheBase& proto)
  : RCObjectInterface()
  , AggMemberDataset(proto)
  , _dimensionCache(proto._dimensionCache)
  {
  }

  AggMemberDatasetWithDimensionCacheBase&
  AggMemberDatasetWithDimensionCacheBase::operator=(
      const AggMemberDatasetWithDimensionCacheBase& rhs)
  {
    if (&rhs != this)
      {
        AggMemberDataset::operator=(rhs);
        _dimensionCache.clear();
        _dimensionCache = rhs._dimensionCache;
      }
    return *this;
  }

  /* virtual */
  unsigned int
  AggMemberDatasetWithDimensionCacheBase::getCachedDimensionSize(
      const std::string& dimName) const
  {
    Dimension* pDim =
        const_cast<AggMemberDatasetWithDimensionCacheBase*>(this)->findDimension(dimName);
    if (pDim)
      {
        return pDim->size;
      }
    else
      {
        std::ostringstream oss;
        oss << "AggMemberDatasetWithDimensionCacheBase::getCachedDimensionSize(): "
            " Dimension "
            << dimName
            << " was not found in the cache!";
        throw DimensionNotFoundException(oss.str());
      }
  }

  /* virtual */
  bool
  AggMemberDatasetWithDimensionCacheBase::isDimensionCached(
      const std::string& dimName) const
  {
    return bool(
        const_cast<AggMemberDatasetWithDimensionCacheBase*>(this)->findDimension(dimName)
        );
  }

  /* virtual */
  void
  AggMemberDatasetWithDimensionCacheBase::setDimensionCacheFor(
      const Dimension& dim,
      bool throwIfFound)
  {
    Dimension* pExistingDim = findDimension(dim.name);
    if (pExistingDim)
      {
        if (!throwIfFound)
          {
            *pExistingDim = dim;
          }
        else
          {
            std::ostringstream msg;
            msg << "AggMemberDatasetWithDimensionCacheBase::setDimensionCacheFor():"
                " Dimension name="
                << dim.name
                << " already exists and we were asked to set uniquely!";
            throw AggregationException(msg.str());
          }
      }
    else
      {
        _dimensionCache.push_back(dim);
      }
  }

  /* virtual */
  void
  AggMemberDatasetWithDimensionCacheBase::fillDimensionCacheByUsingDataDDS()
  {
    // Get the dds
    DataDDS* pDDS = const_cast<DataDDS*>(getDataDDS());
    VALID_PTR(pDDS);

    // Recursive add on all of them
    for (DataDDS::Vars_iter it = pDDS->var_begin();
        it != pDDS->var_end();
        ++it)
      {
        BaseType* pBT = *it;
        VALID_PTR(pBT);
        addDimensionsForVariableRecursive(*pBT);
      }
  }

  /* virtual */
  void
  AggMemberDatasetWithDimensionCacheBase::flushDimensionCache()
  {
    _dimensionCache.clear();
  }

  Dimension*
  AggMemberDatasetWithDimensionCacheBase::findDimension(
      const std::string& dimName)
  {
    Dimension* ret = 0;
    for (vector<Dimension>::iterator it = _dimensionCache.begin();
        it != _dimensionCache.end();
        ++it)
      {
        if (it->name == dimName)
          {
            ret = &(*it);
          }
      }
    return ret;
  }

  void
  AggMemberDatasetWithDimensionCacheBase::addDimensionsForVariableRecursive(libdap::BaseType& var)
  {
    static const string sFuncName = "AggMemberDatasetWithDimensionCacheBase::addDimensionsForVariable():";

    {
      std::ostringstream oss;
      oss << sFuncName
          << " Adding for variable name = "
          << var.name();
      BESDEBUG(DEBUG_CHANNEL, oss.str() << endl);
    }

    if (var.type() == libdap::dods_array_c)
      {
        std::ostringstream oss;
        oss << sFuncName
            << " Adding dimensions for array variable name = "
            << var.name();
        BESDEBUG(DEBUG_CHANNEL, oss.str() << endl);

        libdap::Array& arrVar = dynamic_cast<libdap::Array&>(var);
        libdap::Array::Dim_iter it;
        for (it = arrVar.dim_begin(); it != arrVar.dim_end(); ++it)
          {
            libdap::Array::dimension& dim = *it;
            if (!isDimensionCached(dim.name))
              {
                Dimension newDim(dim.name, dim.size);

                std::ostringstream msg;
                msg << sFuncName
                    << " Adding dimension: "
                    << newDim.toString()
                    << " to the dataset granule cache...";
                BESDEBUG(DEBUG_CHANNEL, msg.str() << endl);

                setDimensionCacheFor(newDim, false);
              }
          }
      }
    // if it's a constructor type, recurse!
    else if (var.is_constructor_type())
      {
        std::ostringstream oss;
        oss << sFuncName
            << " Recursing on all variables for constructor variable name = "
            << var.name();
        BESDEBUG(DEBUG_CHANNEL, oss.str() << endl);

        libdap::Constructor& containerVar = dynamic_cast<libdap::Constructor&>(var);
        libdap::Constructor::Vars_iter it;
        for (it = containerVar.var_begin(); it != containerVar.var_end(); ++it)
          {
            std::ostringstream msg;
            msg << sFuncName
                << " Recursing on variable name="
                << (*it)->name();
            BESDEBUG(DEBUG_CHANNEL, msg.str() << endl);

            addDimensionsForVariableRecursive( *(*it) );
          }
      }
  }

}
