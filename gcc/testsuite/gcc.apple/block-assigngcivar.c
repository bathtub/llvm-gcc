/* APPLE LOCAL file radar 5832193 */
/* assigning a Block into an ivar should elicit a  write-barrier under GC */
/* { dg-do run } */
/* { dg-options "-mmacosx-version-min=10.5 -ObjC -fobjc-gc -framework Foundation" { target *-*-darwin* } } */

#ifdef objc_assign_ivar
#define __objc_assign_ivar objc_assign_ivar
#endif
#include <Foundation/Foundation.h>

#undef objc_assign_ivar

void * _NSConcreteStackBlock;
int GlobalInt = 0;

id objc_assign_global(id val, id *dest) {
    GlobalInt = 1;
    return (id)0;
}

id objc_assign_ivar_Fast(id val, id dest, ptrdiff_t offset) {
    GlobalInt = 1;
    return (id)0;
}

id objc_assign_ivar(id val, id dest, ptrdiff_t offset) {
    GlobalInt = 1;
    return (id)0;
}

id objc_assign_strongCast(id val, id *dest) {
    GlobalInt = 1;
    return (id)0;
}

@interface TestObject : NSObject {
@public
    void (^ivarBlock)(void);
    id x;
}
@end

@implementation TestObject
@end


int main(char *argc, char *argv[]) {
   int i = 0;
   TestObject *to = [[TestObject alloc] init];
   // assigning a Block into an ivar should elicit a  write-barrier under GC
   to->ivarBlock =  ^ { | i | ++i; };		// fails to gen write-barrier  /* { dg-warning "has been deprecated in blocks" } */
   //to->x = to;				// gens write-barrier
   return GlobalInt - 1;
}
