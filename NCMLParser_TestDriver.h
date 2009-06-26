/*
 * NCMLParser_TestDriver.h
 *
 *  Created on: Jun 26, 2009
 *      Author: aries
 */

#ifndef NCMLPARSER_TESTDRIVER_H_
#define NCMLPARSER_TESTDRIVER_H_

#include <string>

namespace ncml_module
{
  class NCMLParser;

  class NCMLParserTestDriver
  {
  public:
    NCMLParserTestDriver();
    virtual ~NCMLParserTestDriver();

    static bool flatDataTestDriver(NCMLParser& parser);
    static bool nestedDataTestDrive(NCMLParser& parser);
    static bool passthroughTest(NCMLParser& parser, const string& locationName);
    static bool attributeStructureTestDriver(NCMLParser& parser);
    static bool existingAttributeStructureTestDriver(NCMLParser &parser);

  }; // class NCMLParserTestDriver

} // namespace ncml_module

#endif /* NCMLPARSER_TESTDRIVER_H_ */
