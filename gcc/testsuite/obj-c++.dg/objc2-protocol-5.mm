/* APPLE LOCAL file 4695109 */
/* { dg-options "-mmacosx-version-min=10.5 -fobjc-abi-version=2" } */
/* { dg-do compile { target *-*-darwin* } } */

@protocol Proto1
+classMethod;
@end

@protocol Proto2
+classMethod2;
@end

int main() {
	return (long) @protocol(Proto1);
}
/* LLVM LOCAL llvm syntax */
/* { dg-final { scan-assembler "L_.*OBJC_PROTOCOL_\\\$_Proto1:" } } */
