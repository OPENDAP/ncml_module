/*
 * NCMLParser_TestDriver.cc
 *
 *  Created on: Jun 26, 2009
 *      Author: aries
 */

#include "config.h"
#include "BESDebug.h"
#include "NCMLParser_TestDriver.h"
#include "NCMLParser.h"

static const string TEST_LOCATION("data/temperature.csv");
static const string TEST_HDF_LOCATION("data/3B42.001003.5.HDF");

namespace ncml_module
{

  NCMLParserTestDriver::NCMLParserTestDriver()
  {
    // TODO Auto-generated constructor stub

  }

  NCMLParserTestDriver::~NCMLParserTestDriver()
  {
    // TODO Auto-generated destructor stub
  }

  bool
  NCMLParserTestDriver::flatDataTestDriver(NCMLParser& parser)
  {
    const string& locationName = TEST_LOCATION;

    // Testing a new global attribute.
    const string globalAttrName("GLOBAL");
    const string globalAttrType("string");
    const string globalAttrValue("Test global attribute!");

    // Testing adding new attribute to existing variable
    const string variable("temperature_K");
    const string newAttrName("units");
    const string newAttrType("String");
    const string newAttrValue("Kelvin");

    // to test changing an existing variable
    const string changeAttrName("type");
    const string changeAttrType("String");  // new type for this variable since it's incorrectly Float32 now with value of "Float32"!!
    const string changeAttrValue("Float32"); // the type name is actually a string.

    parser.handleBeginLocation(locationName);

    // Source metadata directives
    // parser.handleExplicit();
    // parser.handleReadMetadata();

    // Global attributes
    parser.handleBeginAttribute(globalAttrName, globalAttrType, globalAttrValue);
    parser.handleEndAttribute();

    // Test changing attributes in an existing variable
    parser.handleBeginVariable(variable);

    // Test adding a new attribute
    parser.handleBeginAttribute(newAttrName, newAttrType, newAttrValue);
    parser.handleEndAttribute();

    // Test changing an existing attribute.
    parser.handleBeginAttribute(changeAttrName, changeAttrType, changeAttrValue);
    parser.handleEndAttribute();

    parser.handleEndVariable();

    parser.handleEndLocation();
    return true;
  }

  bool
  NCMLParserTestDriver::nestedDataTestDrive(NCMLParser& parser)
  {
    const string& locationName = TEST_HDF_LOCATION;

    parser.handleBeginLocation(locationName);
    parser.handleBeginVariable("DATA_GRANULE");
    parser.handleBeginAttribute("units", "String", "inches");
    parser.handleEndAttribute();
    parser.handleEndVariable();
    parser.handleEndVariable();
    parser.handleEndVariable();
    parser.handleEndLocation();

    return true;
  }

  bool
  NCMLParserTestDriver::passthroughTest(NCMLParser& parser, const string& locationName)
  {
    parser.handleBeginLocation(locationName);
    parser.handleEndLocation();
    return true;
  }

  bool
  NCMLParserTestDriver::attributeStructureTestDriver(NCMLParser& parser)
  {
    const string& locationName = TEST_LOCATION;

    parser.handleBeginLocation(locationName);

    // Nested Global Attributes
    parser.handleBeginAttribute("GLOBAL_CONTAINER", "Structure", "");

    parser.handleBeginAttribute("Atom1", "String", "Atom1_Value");
    parser.handleEndAttribute();
    parser.handleBeginAttribute("Atom2", "String", "Atom2_Value");
    parser.handleEndAttribute();

    parser.handleBeginAttribute("Nest1", "Structure", "");
    parser.handleBeginAttribute("Atom1", "String", "Atom1_Value");
    parser.handleEndAttribute();
    parser.handleBeginAttribute("Atom2", "String", "Atom2_Value");
    parser.handleEndAttribute();
    parser.handleEndAttribute();

    parser.handleEndAttribute();


    // Test adding nested attribute structures inside the scope of a variable
    parser.handleBeginVariable("temperature_K");

    // Test adding a new attribute
    parser.handleBeginAttribute("units", "String", "Kelvin");
    parser.handleEndAttribute();

    // Nested Attributes Within Variable scope
     parser.handleBeginAttribute("SampleInfo", "Structure", "");

     parser.handleBeginAttribute("Atom1", "String", "Atom1_Value");
     parser.handleEndAttribute();
     parser.handleBeginAttribute("Atom2", "String", "Atom2_Value");
     parser.handleEndAttribute();

     parser.handleBeginAttribute("SensorInfo", "Structure", "");
     parser.handleBeginAttribute("Atom1", "String", "Atom1_Value");
     parser.handleEndAttribute();
     parser.handleBeginAttribute("Atom2", "String", "Atom2_Value");
     parser.handleEndAttribute();
     parser.handleEndAttribute();

     // Test adding a new attribute
     parser.handleBeginAttribute("resolution", "String", ".1");
     parser.handleEndAttribute();

     parser.handleEndAttribute();

    parser.handleEndVariable();

    // A few more in the global space to make sure we walk back pointers properly
    parser.handleBeginAttribute("ExtraGlobal", "String", "Atom1_Value");
    parser.handleEndAttribute();


    parser.handleEndLocation();
    return true;
  }

  // this will operate on the TEST_HDF_LOCATION and
  // try to change some of the metadata in the existing structures.
  bool
  NCMLParserTestDriver::existingAttributeStructureTestDriver(NCMLParser &parser)
  {
    const string& locationName = TEST_HDF_LOCATION;

    parser.handleBeginLocation(locationName);

    parser.handleBeginAttribute("CoreMetadata", "Structure", "");

    parser.handleBeginAttribute("OrbitNumber", "Structure", "");
    parser.handleBeginAttribute("Mandatory", "String", "TRUE");  // this is a replacement of FALSE in the orignal set.
    parser.handleEndAttribute();
    parser.handleEndAttribute();

    // here's a new structure we add to the existing structure
    parser.handleBeginAttribute("DocumentInfo", "Structure", "");
    parser.handleBeginAttribute("Version", "String", "Testing NCML Handler!");
    parser.handleEndAttribute();
    parser.handleEndAttribute();

    parser.handleEndAttribute();

    // TODO Maybe add some nested metadata low down in the nested structures too.

    parser.handleEndLocation();
    return true;
  }

}
