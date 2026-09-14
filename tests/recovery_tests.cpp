#include "recovery.hpp"
#include <stdexcept>
#include <iostream>
using css::Recovery;
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
int main() {
    Recovery r;
    // New player arrives before its mesh, then an effect prevents applying.
    r.observe(true,true,true,true,false);
    check(r.pending(),"Lost restore while player initializes");
    r.observe(true,true,true,false,false);
    check(r.due(1000),"Restore was discarded after first observation");
    r.failed(1000); check(!r.due(1499) && r.due(1500),"Transient failure has no bounded retry");
    r.failed(1500); check(!r.due(2499) && r.due(2500),"Repeated failure spins each frame");
    r.clear(); check(!r.due(10000),"Successful restore keeps applying");
    r.observe(true,true,true,false,true); check(r.due(10000),"Same-player stock reset missed");
    r.clear(); r.observe(true,true,true,false,false); check(!r.pending(),"Unknown third-party mesh was reclaimed");
    r.observe(true,true,true,true,false); r.failed(10000);
    r.observe(true,true,true,true,false); check(r.due(10001),"New player inherits old retry delay");
    r.observe(true,false,true,false,true); check(!r.pending(),"Auto-apply disabled was ignored");
    r.observe(false,true,true,true,true); check(!r.pending(),"Disabled CSS restores appearance");
    r.observe(true,true,false,true,true); check(!r.pending(),"Original selection was overwritten");
    r.observe(true,true,true,true,false);
    for(int i=0;i<20;++i) r.failed(20000);
    check(!r.due(27999) && r.due(28000),"Retry backoff exceeds cap");
    std::cout<<"Recovery lifecycle checks passed\n";
}
