/*
 * AggMemberDatasetDimensionCache.h
 *
 *  Created on: Sep 25, 2015
 *      Author: ndp
 */

#ifndef MODULES_NCML_MODULE_AGGMEMBERDATASETDIMENSIONCACHE_H_
#define MODULES_NCML_MODULE_AGGMEMBERDATASETDIMENSIONCACHE_H_

#include "BESFileLockingCache.h"

namespace agg_util
{
// Forward declaration
class AggMemberDataset;

class AggMemberDatasetDimensionCache: public BESFileLockingCache
{
private:
    static AggMemberDatasetDimensionCache * d_instance;
    static void delete_instance();

    string d_dimCacheDir;
    string d_dataRootDir;
    string d_dimCacheFilePrefix;
    unsigned long d_maxCacheSize;

	AggMemberDatasetDimensionCache();
	AggMemberDatasetDimensionCache(const AggMemberDatasetDimensionCache &src);

	bool is_valid(const std::string &cache_file_name, const std::string &dataset_file_name);


    static string getBesDataRootDirFromConfig();
    static string getCacheDirFromConfig();
    static string getDimCachePrefixFromConfig();
    static unsigned long getCacheSizeFromConfig();


protected:

	AggMemberDatasetDimensionCache(const string &data_root_dir, const string &stored_results_subdir, const string &prefix, unsigned long long size);

public:
	static const string CACHE_DIR_KEY;
	static const string PREFIX_KEY;
	static const string SIZE_KEY;
	 // static const string CACHE_CONTROL_FILE;

    static AggMemberDatasetDimensionCache *get_instance(const string &bes_catalog_root_dir, const string &stored_results_subdir, const string &prefix, unsigned long long size);
    static AggMemberDatasetDimensionCache *get_instance();
    static string assemblePath(const string &firstPart, const string &secondPart, bool addLeadingSlash =  false);

    // Overrides parent method
    virtual string get_cache_file_name(const string &src, bool mangle = false);

    // string loadDimensionCache(AggMemberDatasetWithDimensionCacheBase *amd);
    void loadDimensionCache(AggMemberDataset *amd);

	virtual ~AggMemberDatasetDimensionCache();
};

} /* namespace agg_util */

#endif /* MODULES_NCML_MODULE_AGGMEMBERDATASETDIMENSIONCACHE_H_ */
