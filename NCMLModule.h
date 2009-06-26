// NCMLModule.h

#ifndef I_NCMLModule_H
#define I_NCMLModule_H 1

#include "BESAbstractModule.h"

namespace ncml_module
{
class NCMLModule : public BESAbstractModule
{
public:
    				NCMLModule() {}
    virtual		    	~NCMLModule() {}
    virtual void		initialize( const string &modname ) ;
    virtual void		terminate( const string &modname ) ;

    virtual void		dump( ostream &strm ) const ;
} ; // class NCMLModule
} // namespace ncml_module
#endif // I_NCMLModule_H

