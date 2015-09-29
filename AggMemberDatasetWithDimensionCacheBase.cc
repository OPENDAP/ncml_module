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
#include "AggMemberDatasetWithDimensionCacheBase.h"

#include <sstream>
#include <algorithm>
#include <fstream>
#include <sys/stat.h>

#include "AggregationException.h" // agg_util
#include "AggMemberDatasetDimensionCache.h"
#include "Array.h" // libdap
#include "BaseType.h" // libdap
#include "Constructor.h" // libdap
#include "DataDDS.h" // libdap
#include "DDS.h" // libdap
#include "NCMLDebug.h"
#include "TheBESKeys.h"

using std::string;
using libdap::BaseType;
using libdap::Constructor;
using libdap::DataDDS;
using libdap::DDS;

static const string BES_DATA_ROOT("BES.Data.RootDirectory");
static const string BES_CATALOG_ROOT("BES.Catalog.catalog.RootDirectory");


static const string DEBUG_CHANNEL("agg_util");

namespace agg_util {

// Used to init the DimensionCache below with an estimated number of dimensions
static const unsigned int DIMENSION_CACHE_INITIAL_SIZE = 0;

AggMemberDatasetWithDimensionCacheBase::AggMemberDatasetWithDimensionCacheBase(const std::string& location) :
    AggMemberDataset(location), _dimensionCache(DIMENSION_CACHE_INITIAL_SIZE)
{
}

/* virtual */
AggMemberDatasetWithDimensionCacheBase::~AggMemberDatasetWithDimensionCacheBase()
{
    _dimensionCache.clear();
    _dimensionCache.resize(0);
}

AggMemberDatasetWithDimensionCacheBase::AggMemberDatasetWithDimensionCacheBase(
    const AggMemberDatasetWithDimensionCacheBase& proto) :
    RCObjectInterface(), AggMemberDataset(proto), _dimensionCache(proto._dimensionCache)
{
}

AggMemberDatasetWithDimensionCacheBase&
AggMemberDatasetWithDimensionCacheBase::operator=(const AggMemberDatasetWithDimensionCacheBase& rhs)
{
    if (&rhs != this) {
        AggMemberDataset::operator=(rhs);
        _dimensionCache.clear();
        _dimensionCache = rhs._dimensionCache;
    }
    return *this;
}

/* virtual */
unsigned int AggMemberDatasetWithDimensionCacheBase::getCachedDimensionSize(const std::string& dimName) const
{
    Dimension* pDim = const_cast<AggMemberDatasetWithDimensionCacheBase*>(this)->findDimension(dimName);
    if (pDim) {
        return pDim->size;
    }
    else {
        std::ostringstream oss;
        oss << __PRETTY_FUNCTION__ << " Dimension " << dimName << " was not found in the cache!";
        throw DimensionNotFoundException(oss.str());
    }
}

/* virtual */
bool AggMemberDatasetWithDimensionCacheBase::isDimensionCached(const std::string& dimName) const
{
    return bool(const_cast<AggMemberDatasetWithDimensionCacheBase*>(this)->findDimension(dimName));
}

/* virtual */
void AggMemberDatasetWithDimensionCacheBase::setDimensionCacheFor(const Dimension& dim, bool throwIfFound)
{
    Dimension* pExistingDim = findDimension(dim.name);
    if (pExistingDim) {
        if (!throwIfFound) {
            // This discards the object that was in the vector with this name
            // and replaces it with the information passed in via 'dim'. NB:
            // the values of the object referenced by 'dim' are copied into
            // the object pointed to by 'pExistingDim'.
            *pExistingDim = dim;
        }
        else {
            std::ostringstream msg;
            msg << __PRETTY_FUNCTION__ << " Dimension name=" << dim.name
                << " already exists and we were asked to set uniquely!";
            throw AggregationException(msg.str());
        }
    }
    else {
        _dimensionCache.push_back(dim);
    }
}

/* virtual */
void AggMemberDatasetWithDimensionCacheBase::fillDimensionCacheByUsingDataDDS()
{
    // Get the dds
    DataDDS* pDDS = const_cast<DataDDS*>(getDataDDS());
    VALID_PTR(pDDS);

    // Recursive add on all of them
    for (DataDDS::Vars_iter it = pDDS->var_begin(); it != pDDS->var_end(); ++it) {
        BaseType* pBT = *it;
        VALID_PTR(pBT);
        addDimensionsForVariableRecursive(*pBT);
    }
}

/* virtual */
void AggMemberDatasetWithDimensionCacheBase::flushDimensionCache()
{
    _dimensionCache.clear();
}

/* virtual */
void AggMemberDatasetWithDimensionCacheBase::saveDimensionCache(std::ostream& ostr)
{
    saveDimensionCacheInternal(ostr);
}

/* virtual */
void AggMemberDatasetWithDimensionCacheBase::loadDimensionCache(std::istream& istr)
{
    loadDimensionCacheInternal(istr);
}

Dimension*
AggMemberDatasetWithDimensionCacheBase::findDimension(const std::string& dimName)
{
    Dimension* ret = 0;
    for (vector<Dimension>::iterator it = _dimensionCache.begin(); it != _dimensionCache.end(); ++it) {
        if (it->name == dimName) {
            ret = &(*it);
        }
    }
    return ret;
}

void AggMemberDatasetWithDimensionCacheBase::addDimensionsForVariableRecursive(libdap::BaseType& var)
{
    BESDEBUG_FUNC(DEBUG_CHANNEL, "Adding dimensions for variable name=" << var.name() << endl);

    if (var.type() == libdap::dods_array_c) {
        BESDEBUG(DEBUG_CHANNEL, " Adding dimensions for array variable name = " << var.name() << endl);

        libdap::Array& arrVar = dynamic_cast<libdap::Array&>(var);
        libdap::Array::Dim_iter it;
        for (it = arrVar.dim_begin(); it != arrVar.dim_end(); ++it) {
            libdap::Array::dimension& dim = *it;
            if (!isDimensionCached(dim.name)) {
                Dimension newDim(dim.name, dim.size);
                setDimensionCacheFor(newDim, false);

                BESDEBUG(DEBUG_CHANNEL,
                    " Adding dimension: " << newDim.toString() << " to the dataset granule cache..." << endl);
            }
        }
    }

    else if (var.is_constructor_type()) // then recurse
    {
        BESDEBUG(DEBUG_CHANNEL, " Recursing on all variables for constructor variable name = " << var.name() << endl);

        libdap::Constructor& containerVar = dynamic_cast<libdap::Constructor&>(var);
        libdap::Constructor::Vars_iter it;
        for (it = containerVar.var_begin(); it != containerVar.var_end(); ++it) {
            BESDEBUG(DEBUG_CHANNEL, " Recursing on variable name=" << (*it)->name() << endl);

            addDimensionsForVariableRecursive(*(*it));
        }
    }
}

// Sort function
static bool sIsDimNameLessThan(const Dimension& lhs, const Dimension& rhs)
{
    return (lhs.name < rhs.name);
}

void AggMemberDatasetWithDimensionCacheBase::saveDimensionCacheInternal(std::ostream& ostr)
{
    BESDEBUG("agg_util", "Saving dimension cache for dataset location = " << getLocation() << " ..." << endl);

    // Not really necessary, but might help with trying to read output
    std::sort(_dimensionCache.begin(), _dimensionCache.end(), sIsDimNameLessThan);

    // Save out the location first, ASSUMES \n is NOT in the location for read back
    const std::string& loc = getLocation();
    ostr << loc << '\n';

    // Now save each dimension
    unsigned int n = _dimensionCache.size();
    ostr << n << '\n';
    for (unsigned int i = 0; i < n; ++i) {
        const Dimension& dim = _dimensionCache.at(i);
        // @TODO This assumes the dimension names don't contain spaces. We should fix this, and the loader, to work with any name.
        ostr << dim.name << '\n' << dim.size << '\n';
    }
}


void AggMemberDatasetWithDimensionCacheBase::loadDimensionCacheInternal(std::istream& istr)
{
    BESDEBUG("agg_util", "Loading dimension cache for dataset location = " << getLocation() << endl);

    // Read in the location string
    std::string loc;
    getline(istr, loc, '\n');

    // Make sure the location we read is the same as the location
    // for this AMD or there's an unrecoverable serialization bug
    if (loc != getLocation()) {
        stringstream ss;
        ss << "Serialization error: the location loaded from the "
            "dimensions cache was: \"" << loc << "\" but we expected it to be " << getLocation()
            << "\".  Unrecoverable!";
        THROW_NCML_INTERNAL_ERROR(ss.str());
    }

    unsigned int n = 0;
    istr >> n >> ws;
    BESDEBUG("ncml", "AggMemberDatasetWithDimensionCacheBase::loadDimensionCacheInternal() - n: " << n << endl);
    for (unsigned int i = 0; i < n; ++i) {
        Dimension newDim;
        istr >> newDim.name >> ws;
        BESDEBUG("ncml", "AggMemberDatasetWithDimensionCacheBase::loadDimensionCacheInternal() - newDim.name: " << newDim.name << endl);
        istr >> newDim.size >> ws;
        BESDEBUG("ncml", "AggMemberDatasetWithDimensionCacheBase::loadDimensionCacheInternal() - newDim.size: " << newDim.size << endl);
        if (istr.bad()) {
            // Best we can do is throw an internal error for now.
            // Perhaps later throw something else that causes a
            // recreation of the cache
            THROW_NCML_INTERNAL_ERROR("Parsing dimension cache failed to deserialize from stream.");
        }
        _dimensionCache.push_back(newDim);
    }
}

#if 0
//####################################################################################################
//################################### NEW DIMENSION CACHE API ########################################

#define CACHE_DIR "/tmp"
#define CACHE_FILE_PREFIX "/ncml_dimension_cache-"


/**
 * Replace all occurrences of "search" with "replace" in "subject" and return the result.
 */
std::string replaceString(std::string subject, const std::string& search,
                          const std::string& replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
    return subject;
}


/**
 * Returns the file system path defined by the BES.Catalog.catalog.RootDirectory key or, lacking that,
 * the file system path defined by the BES.Data.RootDirectory key. Lacking both this function
 * will thrown an exception.
 */
string getBesCatalogRootDir(){
	bool found;
    string cacheDir = "";
    TheBESKeys::TheKeys()->get_value( BES_CATALOG_ROOT, cacheDir, found ) ;
    if( !found ) {
        TheBESKeys::TheKeys()->get_value( BES_DATA_ROOT, cacheDir, found ) ;
        if( !found ) {
        	string msg = ((string)"[ERROR] getBesCatalogRootDir() - Neither the BES Key ") + BES_CATALOG_ROOT +
        			"or the BES key " + BES_DATA_ROOT + " have been set!";
        	BESDEBUG("ncml", msg << endl);
            throw BESInternalError(msg , __FILE__, __LINE__);
        }
    }
    return cacheDir;
}

/**
 * Returns the file system full qualified path name for the dimension cache file associated with this
 * AMD.
 */
std::string AggMemberDatasetWithDimensionCacheBase::getDimensionCacheFileName(){
	std::string location = replaceString(getLocation()," ","#");
	location = replaceString(location,"/","#");
	std::string cacheFile = ((string)CACHE_DIR) + CACHE_FILE_PREFIX + location;

	BESDEBUG("ncml","AggMemberDatasetWithDimensionCacheBase::getCacheFileName() - cacheFileName: " <<  cacheFile << endl);

	return cacheFile;
}


/**
 * Saves the dimension cache for the AMD to the appropriate disk cache file.
 */
void AggMemberDatasetWithDimensionCacheBase::saveDimensionCacheToDiskCache()
{
	string cacheFileName = getDimensionCacheFileName();
    std::fstream fs (cacheFileName.c_str(), std::fstream::out | std::fstream::trunc);
    if(fs.fail()){
    	string msg = "AggregationElement::cacheGranulesDimensions() - OUCH! Failed to open " + cacheFileName;
        BESDEBUG("ncml", msg << endl);
        throw BESInternalError(msg , __FILE__, __LINE__);
    }
    else {
		saveDimensionCacheInternal(fs);
    }
    fs.close();
}

/**
 * Loads the dimension cache for this AMD from the appropriate cache file.
 */
void AggMemberDatasetWithDimensionCacheBase::loadDimensionCacheFromDiskCache()
{
	string cacheFileName = getDimensionCacheFileName();
    std::fstream fs (cacheFileName.c_str(), std::fstream::in);

    if(fs.fail()){
    	string msg = "AggregationElement::cacheGranulesDimensions() - OUCH! Failed to open " + cacheFileName;
        BESDEBUG("ncml", msg << endl);
        throw BESInternalError(msg , __FILE__, __LINE__);
    }
    else {
    	loadDimensionCacheInternal(fs);
    }
    fs.close();
}


/**
 * Looks at the appropriate disk cache file for this AMD's dimension cache. If it's missing, or, if it's
 * Last Modified Time is older (less than) the LMT of the source data file this method returns true, false
 * otherwise.
 */
bool AggMemberDatasetWithDimensionCacheBase::dimensionCacheFileNeedsUpdate(){


	std::string cacheFile = getDimensionCacheFileName();

    struct stat statBufCache;
    if (stat( cacheFile.c_str(), &statBufCache) != 0)
    {
    	// File is missing, needs updated!
    	return true;
    }

    std::string catalogRoot = getBesCatalogRootDir();
    std::string sourceDataFile = catalogRoot + "/" + getLocation();

    struct stat statBufSource;
    if (stat( sourceDataFile.c_str(), &statBufSource) != 0)
    {
    	// Source file is missing! This should never happen!
    	string msg = ((string)"[ERROR] AggMemberDatasetWithDimensionCacheBase::dimensionCacheFileNeedsUpdate() - "
    			"Unable to located source data file: ")+sourceDataFile;
    	BESDEBUG("ncml", msg << endl);
        throw BESInternalError(msg , __FILE__, __LINE__);
    }

    time_t cacheLMT = statBufCache.st_mtime;
    time_t sourceLMT = statBufSource.st_mtime;

    // If the source data file has a newer LMT than the dimension cache file then
    // the dimension cache file needs updating. Otherwise not so much (false)
    return cacheLMT < sourceLMT;

}



/**
 * This method will load this AMD's dimension cache. It will attempt to locate it in the appropriate disk
 * cache file. If that file is missing or older than the source data file for the AMD this will cause the
 * the source data file to be interrogated (via it's DDS) and the dimension cache will be (re)built and then
 * saved into the appropriate disk cache file for future use.
 */
void AggMemberDatasetWithDimensionCacheBase::loadDimensionCache()
{
	AggMemberDatasetDimensionCache *cache = AggMemberDatasetDimensionCache::get_instance();

	cache->loadDimensionCache(this);

/*
	if(dimensionCacheFileNeedsUpdate()){
		fillDimensionCacheByUsingDataDDS();
		saveDimensionCacheToDiskCache();
	}
	else {
		loadDimensionCacheFromDiskCache();
	}
*/
}
#endif

}
