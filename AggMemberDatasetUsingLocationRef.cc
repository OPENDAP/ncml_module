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
#include "AggMemberDatasetUsingLocationRef.h"

#include "BESDataDDSResponse.h" // bes
#include "DataDDS.h"
#include "DDS.h" // libdap
#include "NCMLDebug.h" // ncml_module
#include "NCMLUtil.h" // ncml_module

namespace agg_util
{

  AggMemberDatasetUsingLocationRef::AggMemberDatasetUsingLocationRef(
      const std::string& locationToLoad,
      const agg_util::DDSLoader& loaderToUse)
  : AggMemberDataset(locationToLoad)
  , _loader(loaderToUse)
  , _pDataResponse(0)
  {
  }

  AggMemberDatasetUsingLocationRef::~AggMemberDatasetUsingLocationRef()
  {
    cleanup();
  }

  AggMemberDatasetUsingLocationRef::AggMemberDatasetUsingLocationRef(const AggMemberDatasetUsingLocationRef& proto)
  : RCObjectInterface()
  , AggMemberDataset(proto)
  , _loader(proto._loader)
  , _pDataResponse(0) // force a reload as needed for a copy
  {
  }

  AggMemberDatasetUsingLocationRef&
  AggMemberDatasetUsingLocationRef::operator=(const AggMemberDatasetUsingLocationRef& that)
  {
    if (this != &that)
      {
        // clear out any old loaded stuff
        cleanup();
        // assign
        AggMemberDataset::operator=(that);
        copyRepFrom(that);
      }
    return *this;
  }


  const libdap::DataDDS*
  AggMemberDatasetUsingLocationRef::getDataDDS()
  {
    if (!_pDataResponse)
      {
        loadDataDDS();
      }
    DataDDS* pDDSRet = 0;
    if (_pDataResponse)
      {
        pDDSRet = _pDataResponse->get_dds();
      }
    return pDDSRet;
  }

  /////////////////////////////// Private Helpers ////////////////////////////////////
  void
  AggMemberDatasetUsingLocationRef::loadDataDDS()
  {
    // We cannot load an empty location, so avoid the exception later.
    if (getLocation().empty())
      {
        THROW_NCML_INTERNAL_ERROR("AggMemberDatasetUsingLocationRef():"
            " got empty location!  Cannot load!");
      }

    // Make a new response and store the raw ptr, noting that we need to delete it in dtor.
    std::auto_ptr<BESDapResponse> newResponse = _loader.makeResponseForType(DDSLoader::eRT_RequestDataDDS);
    VALID_PTR(newResponse.get());
    // static_cast should work here, but I want to be sure since DataDDX is in the works...
    _pDataResponse = dynamic_cast<BESDataDDSResponse*>(newResponse.get());
    NCML_ASSERT_MSG(_pDataResponse,
        "AggMemberDatasetUsingLocationRef::loadDDS(): "
        " failed to get a BESDataDDSResponse back"
        " while loading location=" + getLocation());
    // release after potential for exception to avoid leak
    newResponse.release();

    BESDEBUG("ncml", "Loading DataDDS for aggregation member location = " << getLocation() << endl);
    _loader.loadInto(getLocation(), DDSLoader::eRT_RequestDataDDS, _pDataResponse);
  }

  void
  AggMemberDatasetUsingLocationRef::cleanup() throw()
  {
    SAFE_DELETE(_pDataResponse);
  }

  void
  AggMemberDatasetUsingLocationRef::copyRepFrom(const AggMemberDatasetUsingLocationRef& rhs)
  {
    _loader = rhs._loader;
    _pDataResponse = 0; // force this to be NULL... we want to reload if we get an assignment
  }

}