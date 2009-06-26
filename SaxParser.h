
#ifndef SAXPARSER_H_
#define SAXPARSER_H_

#include <string>
#include <map>

namespace ncml_module
{
  /**
   * Interface class for the wrapper between libxml C parser and our NCMLParser engine.
   * Also contains definition for AttrMap, which is how the attrs will be returned to the parser.
   * @author mjohnson <m.johnson@opendap.org>
   */
class SaxParser
{
protected:
  SaxParser(); // Interface class

public:
  /** Attributes will be shoved into a map for calls below. */
  typedef std::map<std::string, std::string> AttrMap;

  /** @brief Return the value of the given attribute from the map, else the given default.
   * @param map  map to search
   * @param name name of the attribute to find
   * @param default  what to return if name not found
   * @return the attribute value or default as a const reference.
   */
  static const std::string& findAttrValue(const SaxParser::AttrMap& map, const std::string& name, const std::string& def="");

  virtual ~SaxParser() {};

  virtual bool onStartDocument() = 0;
  virtual bool onEndDocument() = 0;

  /** Called at the start of the element with the given name and attribute dictionary
    *  The args are only valid for the duration of the call, so copy if necessary to keep.
    * */
  virtual bool onStartElement(const std::string& name, const AttrMap& attrs) = 0;

  /** Called at the end of the element with the given name.
    *  The args are only valid for the duration of the call, so copy if necessary to keep.
    * */
  virtual bool onEndElement(const std::string& name) = 0;

  /** Called when characters are encountered within an element.
   * content is only valid for the call duration.
   */
  virtual bool onCharacters(const std::string& content) = 0;

  /** A recoverable parse error occured. */
  virtual bool onParseWarning(std::string msg) = 0;

  /** An unrecoverable parse error occurred */
  virtual bool onParseError(std::string msg) = 0;

}; // class SaxParser

} // namespace ncml_module

#endif /* SAXPARSER_H_ */
