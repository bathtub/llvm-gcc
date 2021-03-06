/* A test for final value replacement.  */

/* { dg-options "-O2 -fdump-tree-optimized" } */
/* LLVM LOCAL test not applicable */
/* { dg-require-fdump "" } */

int foo(void);

int bla(void)
{
  int i, j = foo ();

  for (i = 0; i < 100; i++, j++)
    foo ();

  /* Should be replaced with return j0 + 100;  */
  return j;
}

/* { dg-final { scan-tree-dump-times "\\+ 100" 1 "optimized" } } */
/* { dg-final { cleanup-tree-dump "optimized" } } */
