/*
 * AggMemberDatasetDimensionCache.cc
 *
 *  Created on: Sep 25, 2015
 *      Author: ndp
 */

#include "AggMemberDatasetDimensionCache.h"
#include "AggMemberDataset.h"
#include <string>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

#include "util.h"
#include "BESInternalError.h"
#include "BESDebug.h"
#include "TheBESKeys.h"


static const string BES_DATA_ROOT("BES.Data.RootDirectory");
static const string BES_CATALOG_ROOT("BES.Catalog.catalog.RootDirectory");


namespace agg_util
{

AggMemberDatasetDimensionCache *AggMemberDatasetDimensionCache::d_instance = 0;
const string AggMemberDatasetDimensionCache::CACHE_DIR_KEY = "NCML.DimensionCache.directory";
const string AggMemberDatasetDimensionCache::PREFIX_KEY    = "NCML.DimensionCache.prefix";
const string AggMemberDatasetDimensionCache::SIZE_KEY      = "NCML.DimensionCache.size";

unsigned long AggMemberDatasetDimensionCache::getCacheSizeFromConfig(){

	bool found;
    string size;
    unsigned long size_in_megabytes = 0;
    TheBESKeys::TheKeys()->get_value( SIZE_KEY, size, found ) ;
    if( found ) {
    	std::istringstream iss(size);
    	iss >> size_in_megabytes;
    }
    else {
    	string msg = "[ERROR] AggMemberDatasetDimensionCache::getCacheSize() - The BES Key " + SIZE_KEY + " is not set! It MUST be set to utilize the NcML Dimension Cache. ";
    	BESDEBUG("cache", msg << endl);
        throw BESInternalError(msg , __FILE__, __LINE__);
    }
    return size_in_megabytes;
}

string AggMemberDatasetDimensionCache::getCacheDirFromConfig(){
	bool found;
    string subdir = "";
    TheBESKeys::TheKeys()->get_value( CACHE_DIR_KEY, subdir, found ) ;

	if( !found ) {
    	string msg = "[ERROR] AggMemberDatasetDimensionCache::getSubDirFromConfig() - The BES Key " + CACHE_DIR_KEY + " is not set! It MUST be set to utilize the NcML Dimension Cache. ";
    	BESDEBUG("cache", msg << endl);
        throw BESInternalError(msg , __FILE__, __LINE__);
	}
	else {
		while(*subdir.begin() == '/' && subdir.length()>0){
			subdir = subdir.substr(1);
		}
		// So if it's value is "/" or the empty string then the subdir will default to the root
		// directory of the BES data system.
	}

    return subdir;
}


string AggMemberDatasetDimensionCache::getDimCachePrefixFromConfig(){
	bool found;
    string prefix = "";
    TheBESKeys::TheKeys()->get_value( PREFIX_KEY, prefix, found ) ;
	if( found ) {
		prefix = BESUtil::lowercase( prefix ) ;
	}
	else {
    	string msg = "[ERROR] AggMemberDatasetDimensionCache::getResultPrefix() - The BES Key " + PREFIX_KEY + " is not set! It MUST be set to utilize the NcML Dimension Cache. ";
    	BESDEBUG("cache", msg << endl);
        throw BESInternalError(msg , __FILE__, __LINE__);
	}

    return prefix;
}


string AggMemberDatasetDimensionCache::getBesDataRootDirFromConfig(){
	bool found;
    string cacheDir = "";
    TheBESKeys::TheKeys()->get_value( BES_CATALOG_ROOT, cacheDir, found ) ;
    if( !found ) {
        TheBESKeys::TheKeys()->get_value( BES_DATA_ROOT, cacheDir, found ) ;
        if( !found ) {
        	string msg = ((string)"[ERROR] AggMemberDatasetDimensionCache::getStoredResultsDir() - Neither the BES Key ") + BES_CATALOG_ROOT +
        			"or the BES key " + BES_DATA_ROOT + " have been set! One MUST be set to utilize the NcML Dimension Cache. ";
        	BESDEBUG("cache", msg << endl);
            throw BESInternalError(msg , __FILE__, __LINE__);
        }
    }
    return cacheDir;

}

AggMemberDatasetDimensionCache::AggMemberDatasetDimensionCache()
{
	BESDEBUG("cache", "AggMemberDatasetDimensionCache::AggMemberDatasetDimensionCache() -  BEGIN" << endl);

	d_dimCacheDir = getCacheDirFromConfig();
	d_dataRootDir = getBesDataRootDirFromConfig();

    d_dimCacheFilePrefix = getDimCachePrefixFromConfig();
    d_maxCacheSize = getCacheSizeFromConfig();

    BESDEBUG("cache", "AggMemberDatasetDimensionCache() - Stored results cache configuration params: " << d_dimCacheDir << ", " << d_dimCacheFilePrefix << ", " << d_maxCacheSize << endl);

  	initialize(d_dimCacheDir, d_dimCacheFilePrefix, d_maxCacheSize);

    BESDEBUG("cache", "AggMemberDatasetDimensionCache::AggMemberDatasetDimensionCache() -  END" << endl);

}
AggMemberDatasetDimensionCache::AggMemberDatasetDimensionCache(const string &data_root_dir, const string &cache_dir, const string &prefix, unsigned long long size){

	BESDEBUG("cache", "AggMemberDatasetDimensionCache::AggMemberDatasetDimensionCache() -  BEGIN" << endl);

	d_dataRootDir = data_root_dir;
	d_dimCacheDir = cache_dir;
	d_dimCacheFilePrefix = prefix;
	d_maxCacheSize = size;

  	initialize(d_dimCacheDir, d_dimCacheFilePrefix, d_maxCacheSize);

  	BESDEBUG("cache", "AggMemberDatasetDimensionCache::AggMemberDatasetDimensionCache() -  END" << endl);
}



AggMemberDatasetDimensionCache *
AggMemberDatasetDimensionCache::get_instance(const string &data_root_dir, const string &cache_dir, const string &result_file_prefix, unsigned long long max_cache_size)
{
    if (d_instance == 0){
        if (libdap::dir_exists(cache_dir)) {
        	try {
                d_instance = new AggMemberDatasetDimensionCache(data_root_dir, cache_dir, result_file_prefix, max_cache_size);
        	}
        	catch(BESInternalError &bie){
        	    BESDEBUG("cache", "[ERROR] AggMemberDatasetDimensionCache::get_instance(): Failed to obtain cache! msg: " << bie.get_message() << endl);
        	}
    	}
    }
    return d_instance;
}

/** Get the default instance of the AggMemberDatasetDimensionCache object. This will read "TheBESKeys" looking for the values
 * of SUBDIR_KEY, PREFIX_KEY, an SIZE_KEY to initialize the cache.
 */
AggMemberDatasetDimensionCache *
AggMemberDatasetDimensionCache::get_instance()
{
    if (d_instance == 0) {
		try {
			d_instance = new AggMemberDatasetDimensionCache();
		}
		catch(BESInternalError &bie){
			BESDEBUG("cache", "[ERROR] AggMemberDatasetDimensionCache::get_instance(): Failed to obtain cache! msg: " << bie.get_message() << endl);
		}
    }

    return d_instance;
}


void AggMemberDatasetDimensionCache::delete_instance() {
    BESDEBUG("cache","AggMemberDatasetDimensionCache::delete_instance() - Deleting singleton BESStoredDapResultCache instance." << endl);
    delete d_instance;
    d_instance = 0;
}







AggMemberDatasetDimensionCache::~AggMemberDatasetDimensionCache()
{
	// TODO Auto-generated destructor stub
}

/**
 * Is the item named by cache_entry_name valid? This code tests that the
 * cache entry is non-zero in size (returns false if that is the case, although
 * that might not be correct) and that the dataset associated with this
 * ResponseBulder instance is at least as old as the cached entry.
 *
 * @param cache_file_name File name of the cached entry
 * @param local_id The id, relative to the BES Catalog/Data root of the source dataset.
 * @return True if the thing is valid, false otherwise.
 */
bool AggMemberDatasetDimensionCache::is_valid(const string &cache_file_name, const string &local_id)
{
    // If the cached response is zero bytes in size, it's not valid.
    // (hmmm...)
	string datasetFileName = assemblePath(d_dataRootDir,local_id, true);

    off_t entry_size = 0;
    time_t entry_time = 0;
    struct stat buf;
    if (stat(cache_file_name.c_str(), &buf) == 0) {
        entry_size = buf.st_size;
        entry_time = buf.st_mtime;
    }
    else {
        return false;
    }

    if (entry_size == 0)
        return false;

    time_t dataset_time = entry_time;
    if (stat(datasetFileName.c_str(), &buf) == 0) {
        dataset_time = buf.st_mtime;
    }

    // Trick: if the d_dataset is not a file, stat() returns error and
    // the times stay equal and the code uses the cache entry.

    // TODO Fix this so that the code can get a LMT from the correct handler.
    // TODO Consider adding a getLastModified() method to the libdap::DDS object to support this
    // TODO The DDS may be expensive to instantiate - I think the handler may be a better location for an LMT method, if we can access the handler when/where needed.
    if (dataset_time > entry_time)
        return false;

    return true;
}

string AggMemberDatasetDimensionCache::assemblePath(const string &firstPart, const string &secondPart, bool addLeadingSlash){

	//BESDEBUG("cache", "AggMemberDatasetDimensionCache::assemblePath() -  BEGIN" << endl);
	//BESDEBUG("cache", "AggMemberDatasetDimensionCache::assemblePath() -  firstPart: "<< firstPart << endl);
	//BESDEBUG("cache", "AggMemberDatasetDimensionCache::assemblePath() -  secondPart: "<< secondPart << endl);

	string firstPathFragment = firstPart;
	string secondPathFragment = secondPart;


	if(addLeadingSlash){
	    if(*firstPathFragment.begin() != '/')
	    	firstPathFragment = "/" + firstPathFragment;
	}

	// make sure there are not multiple slashes at the end of the first part...
	while(*firstPathFragment.rbegin() == '/' && firstPathFragment.length()>0){
		firstPathFragment = firstPathFragment.substr(0,firstPathFragment.length()-1);
		//BESDEBUG("cache", "AggMemberDatasetDimensionCache::assemblePath() -  firstPathFragment: "<< firstPathFragment << endl);
	}

	// make sure first part ends with a "/"
    if(*firstPathFragment.rbegin() != '/'){
    	firstPathFragment += "/";
    }
	//BESDEBUG("cache", "AggMemberDatasetDimensionCache::assemblePath() -  firstPathFragment: "<< firstPathFragment << endl);

	// make sure second part does not BEGIN with a slash
	while(*secondPathFragment.begin() == '/' && secondPathFragment.length()>0){
		secondPathFragment = secondPathFragment.substr(1);
	}

	//BESDEBUG("cache", "AggMemberDatasetDimensionCache::assemblePath() -  secondPathFragment: "<< secondPathFragment << endl);

	string newPath = firstPathFragment + secondPathFragment;

	//BESDEBUG("cache", "AggMemberDatasetDimensionCache::assemblePath() -  newPath: "<< newPath << endl);
	//BESDEBUG("cache", "AggMemberDatasetDimensionCache::assemblePath() -  END" << endl);

	return newPath;
}


/**
 * Returns the full qualified file system path name for the dimension cache file associated with this
 * AMD.
 */
string AggMemberDatasetDimensionCache::get_cache_file_name(const string &local_id, bool mangle)
{

	BESDEBUG("cache", "AggMemberDatasetDimensionCache::get_cache_file_name() - Starting with local_id: " << local_id << endl);

	std::string cacheFileName(local_id);

	cacheFileName = assemblePath( getCacheFilePrefix(), cacheFileName);

	if(mangle){
		std::replace( cacheFileName.begin(), cacheFileName.end(), ' ', '#'); // replace all ' ' with '#'
		std::replace( cacheFileName.begin(), cacheFileName.end(), '/', '#'); // replace all '/' with '#'
	}

	cacheFileName = assemblePath( getCacheDirectory(),  cacheFileName,true);

	BESDEBUG("cache","AggMemberDatasetDimensionCache::get_cache_file_name() - cacheFileName: " <<  cacheFileName << endl);

	return cacheFileName;
}


/**
 *
 * @return The local ID (relative to the BES data root directory) of the stored dataset.
 */
string AggMemberDatasetDimensionCache::loadDimensionCache(AggMemberDataset *amd){
    BESDEBUG("cache", "AggMemberDatasetDimensionCache::loadDimensionCache() - BEGIN" << endl );

    // Get the cache filename for this thing, mangle name.
    string local_id = amd->getLocation();
    BESDEBUG("cache", "AggMemberDatasetDimensionCache::loadDimensionCache() - local resource id: "<< local_id << endl );
    string cache_file_name = get_cache_file_name(local_id, true);
    BESDEBUG("cache", "AggMemberDatasetDimensionCache::loadDimensionCache() - cache_file_name: "<< cache_file_name << endl );

    int fd;
    try {
        // If the object in the cache is not valid, remove it. The read_lock will
        // then fail and the code will drop down to the create_and_lock() call.
        // is_valid() tests for a non-zero length cache file (cache_file_name) and
    	// for the source data file (local_id) with a newer LMT than the cache file.
        if (!is_valid(cache_file_name, local_id)){
            BESDEBUG("cache", "AggMemberDatasetDimensionCache::loadDimensionCache() - File is not valid. Purging file from cache. filename: " << cache_file_name << endl);
        	purge_file(cache_file_name);
        }

        if (get_read_lock(cache_file_name, fd)) {
            BESDEBUG("cache", "AggMemberDatasetDimensionCache::loadDimensionCache() - Dimension cache file exists. Loading dimension cache from file: " << cache_file_name << endl);

            ifstream istrm(cache_file_name.c_str());
            if (!istrm)
            	throw libdap::InternalErr(__FILE__, __LINE__, "Could not open '" + cache_file_name + "' to read cached dimensions.");

            amd->loadDimensionCache(istrm);

            istrm.close();


        }
        else {
			// If here, the cache_file_name could not be locked for read access, or it was out of date.
        	// So we are going to (re)build the cache file.

        	// We need to build the DDS object and extract the dimensions.
        	// We do not lock before this operation because it may take a _long_ time and
        	// we don't want to monopolize the cache while we do it.
        	amd->fillDimensionCacheByUsingDataDDS();

        	// Now, we try to make an empty cache file and get an exclusive lock on it.
        	if (create_and_lock(cache_file_name, fd)) {
        		// Woohoo! We got the exclusive lock on the new cache file.
				BESDEBUG("cache", "AggMemberDatasetDimensionCache::loadDimensionCache() - Created and locked cache file: " << cache_file_name << endl);

				// Now we open it (again) using the more friendly ostream API.
				ofstream ostrm(cache_file_name.c_str());
				if (!ostrm)
					throw libdap::InternalErr(__FILE__, __LINE__, "Could not open '" + cache_file_name + "' to write cached response.");

				// Save the dimensions to the cache file.
				amd->saveDimensionCache(ostrm);

		        // And close the cache file;s ostream.
				ostrm.close();

				// Change the exclusive lock on the new file to a shared lock. This keeps
				// other processes from purging the new file and ensures that the reading
				// process can use it.
				exclusive_to_shared_lock(fd);

				// Now update the total cache size info and purge if needed. The new file's
				// name is passed into the purge method because this process cannot detect its
				// own lock on the file.
				unsigned long long size = update_cache_info(cache_file_name);
				if (cache_too_big(size))
					update_and_purge(cache_file_name);
			}
			// get_read_lock() returns immediately if the file does not exist,
			// but blocks waiting to get a shared lock if the file does exist.
			else if (get_read_lock(cache_file_name, fd)) {
				// If we got here then someone else rebuilt the cache file before we could do it.
				// That's OK, and since we already built the DDS we have all of the cache info in memory
				// from directly accessing the source dataset(s), so we need to do nothing more,
				// Except send a debug statement so we can see that this happened.
				BESDEBUG("cache", "AggMemberDatasetDimensionCache::loadDimensionCache() - Couldn't create and lock cache file, But I got a read lock. "
						"Cache file may have been rebuilt by another process. "
						"Cache file: " << cache_file_name << endl);
			}
			else {
				throw libdap::InternalErr(__FILE__, __LINE__, "AggMemberDatasetDimensionCache::loadDimensionCache() - Cache error during function invocation.");
			}
        }

        BESDEBUG("cache", "AggMemberDatasetDimensionCache::loadDimensionCache() - unlocking and closing cache file "<< cache_file_name  << endl );
		unlock_and_close(cache_file_name);
    }
    catch (...) {
        BESDEBUG("cache", "AggMemberDatasetDimensionCache::loadDimensionCache() - caught exception, unlocking cache and re-throw." << endl );
        unlock_cache();
        throw;
    }

    BESDEBUG("cache", "AggMemberDatasetDimensionCache::loadDimensionCache() - END (local_id=`"<< local_id << "')" << endl );
    return local_id;

}






} /* namespace agg_util */
