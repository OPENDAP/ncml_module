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
#include "AggMemberDataset.h"

#include "BESDataDDSResponse.h" // bes
#include "DataDDS.h"
#include "DDS.h" // libdap
#include "DDSLoader.h" // agg_util
#include "NCMLDebug.h" // ncml_module
#include "NCMLUtil.h" // ncml_module

namespace agg_util
{

  AggMemberDataset::AggMemberDataset(const string& location)
  : _location(location)
  , _pDataResponse(0)
  {
  }

  AggMemberDataset::AggMemberDataset(const AggMemberDataset& proto)
  : _location(proto._location)
    , _pDataResponse(0) // we CANNOT copy this.  They will have to reload it.
  {
  }

  AggMemberDataset::~AggMemberDataset()
  {
    cleanup();
  }

  AggMemberDataset&
  AggMemberDataset::operator=(const AggMemberDataset& rhs)
  {
    if (&rhs == this)
      {
        return *this;
      }
    cleanup();

    _location = rhs._location;
    // the response is null... they must reload it.
    return *this;
  }

  void
  AggMemberDataset::loadDataDDS(DDSLoader& loader)
  {
    // Make a new response and store the raw ptr, noting that we need to delete it in dtor.
    std::auto_ptr<BESDapResponse> newResponse = loader.makeResponseForType(DDSLoader::eRT_RequestDataDDS);
    VALID_PTR(newResponse.get());
    // static_cast should work here, but I want to be sure since DataDDX is in the works...
    _pDataResponse = dynamic_cast<BESDataDDSResponse*>(newResponse.release());
    NCML_ASSERT_MSG(_pDataResponse, "AggMemberDataset::loadDDS: failed to get a BESDataDDSResponse back"
        " while loading location=" + _location);
    BESDEBUG("ncml", "Loading DataDDS for aggregation member location = " << _location << endl);
    loader.loadInto(_location, DDSLoader::eRT_RequestDataDDS, _pDataResponse);
  }

  libdap::DataDDS*
  AggMemberDataset::getDataDDS() const
  {
    DataDDS* pDDSRet = 0;
    if (_pDataResponse)
      {
        pDDSRet = _pDataResponse->get_dds();
      }
    return pDDSRet;
  }

  void
  AggMemberDataset::clearDataDDS()
  {
    SAFE_DELETE(_pDataResponse);
  }

  void
  AggMemberDataset::cleanup() throw()
  {
    SAFE_DELETE(_pDataResponse);
  }

}
