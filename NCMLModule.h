// NCMLModule.h

#ifndef I_NCMLModule_H
#define I_NCMLModule_H 1

#include "BESAbstractModule.h"

class NCMLModule : public BESAbstractModule
{
public:
    				NCMLModule() {}
    virtual		    	~NCMLModule() {}
    virtual void		initialize( const string &modname ) ;
    virtual void		terminate( const string &modname ) ;

    virtual void		dump( ostream &strm ) const ;
} ;

#endif // I_NCMLModule_H

