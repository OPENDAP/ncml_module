

$Id: README $

Updated for version 0.9.0 (27 July 2009)

Welcome to the OPeNDAP NcML Data Handler Module for Hyrax!

This module can be added to a Hyrax installation to extend its dataserving capability to NcML 2.2 files (seehttp://www.unidata.ucar.edu/software/ne	tcdf/ncml/) which provide a wayto add metadata to existing datasets.

For information on how to build and install the NcML Data Module,please see the INSTALL file.

More detailed and/or up-to-date documentation for this module can befound at: http://docs.opendap.org/index.php/BES_-_Modules_-_NcML_Module



The initial version implements only a subset of NcML 2.2functionality, and also adds functionality -- please see the sections"Functionality" and "NcML Additions" below.

----------------------------------------------------------------------* Contents

  * Installation Overview  * Functionality  * NcML Additions  * Example NcML  * HDF4 DAS Compatibility Issue  * Copyright

----------------------------------------------------------------------* Installation Overview

The NcML Module requires a working Hyrax installation.  It is a modulethat is dynamically loaded into the Hyrax BES (Back End Server) toallow it to handle NcML files.  

Please see the file INSTALL for full build and install instructions aswell as requirements.

Once installed, the BES configuration file (bes.conf) needs to bemodified to tell it to load the module and where it was installed.  Aconfiguration helper script (bes-ncml-data.sh) is installed with themodule to automate this process.  The script is used on the commandline (> prompt) as follows:

   > bes-ncml-data.sh  <bes.conf file to modify>  <bes modules dir>  

If you are doing a source installation, the `bes-conf' make targetruns the script while trying to select paths cleverly, and should becalled using:

   > make bes-conf

After installation, you MUST restart Hyrax by restarting the BES andOLFS so the NcML Module is loaded.

Test data is provided to see if the installation was successful(fnoc1_improved.ncml), but it requires the netCDF data handler to beinstalled since it adds metadata to a netCDF dataset (fnoc1.nc).

----------------------------------------------------------------------* Functionality

This version of the NcML Module implements a subset of NcMLfunctionality.  It can:

     1) Add metadata only to files being served locally (not remotely)     2) Add metadata to only one dataset (one <netcdf> node).     3) Add, modify, and remove metadata (attributes) and not data (variables).

In particular, this version of the NcML module can only handle asubset of the NcML schema.  First, it is restricted to a subset of theelements: <netcdf>, <explicit>, <readMetadata>, <attribute>,<variable>, and <remove>.

The <variable> element can not be used to add data, but only be usedto provide the lexical scope for an <attribute> element.  For example:

<variable name="u">  <attribute name="Metadata" type="string">This is metadata!</attribute></variable>

assumes that a variable named "u" exists (of any type) and theattribute "Metadata" will refer to the metadata for that variable.These can be nested.

Also, the <remove/> element can only remove attributes and notvariables.

We plan to add more features and NcML functionality, such as theability to add and remove variables and to aggregate data in futureversions.

----------------------------------------------------------------------* NcML Additions

This module also adds functionality beyond the current NcML 2.2 schema--- it can handle nested <attribute> elements in order to makeattribute structures.  This is done by using the <attributetype="Structure"> form:

<attribute name="MySamples" type="Structure">  <attribute name="Location" type="string" value="Station 1"/>  <attribute name="Samples" type="int">1 4 6</attribute></attribute>

"MyContainer" is now an attribute structure with two attribute fields,a string and an array of int's.  Note that an attribute structure canonly contain other <attribute> elements and NOT a value.

Additionally, DAP2 atomic types (such as UInt32, URL) can also be usedin addition to the NcML types.  The NcML types are mapped onto theclosest DAP2 type internally.  

----------------------------------------------------------------------* Example NcML

Example NcML files used by the test-suite can be found in the datadirectory of a src distribution.

----------------------------------------------------------------------* HDF4 DAS Compatibility Issue

There is a bug in the Hyrax HDF4 Module such that the DAS produced inincorrect.  If an NcML file is used to "wrap" an HDF4 dataset, thecorrect DAP2 DAS response will be generated.  

This is important for those writing NcML for HDF4 data since thelexical scope for attributes relies on the correct DAS form --- tohandle this, the user should start with a "passthrough" NcML file anduse the DAS from that as the starting point for knowing the structurethe NcML handler expects to see in the NcML file.

----------------------------------------------------------------------* Copyright

This software is copyrighted under the GNU Lesser GPL.  Please see thefiles COPYING and COPYRIGHT that came with this distribution.



































