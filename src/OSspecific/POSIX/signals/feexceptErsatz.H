// ============================================================================
// Generic handling of floating point exceptions
//
// 2018 Alexey Matveichev
// ----------------------------------------------------------------------------
//
// Original Author
//     David N. Williams
//
// License
//     Public Domain
//
// Description
//     Alternate, nondefault handling of IEEE 754 floating-point exceptions
//     in OS X, based on the GNU functions feenableexcept(), fedisableeexcept()
//     and fegetexcept() [in libm]
//
// http://www-personal.umich.edu/~williams/archive/computation/fe-handling-example.c
// ============================================================================

#ifndef feexceptErsatz_H
#define feexceptErsatz_H
#ifdef darwin

#include <fenv.h>

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

inline int feenableexcept(unsigned int excepts)
{
    static fenv_t fenv;
    unsigned int new_excepts = excepts & FE_ALL_EXCEPT;
    unsigned int old_excepts;   // previous masks

    if (fegetenv(&fenv))
    {
        return -1;
    }
    old_excepts = fenv.__control & FE_ALL_EXCEPT;

    // unmask
    fenv.__control &= ~new_excepts;
    fenv.__mxcsr   &= ~(new_excepts << 7);

    return fesetenv(&fenv) ? -1 : old_excepts;
}


inline int fedisableexcept(unsigned int excepts)
{
    static fenv_t fenv;
    unsigned int new_excepts = excepts & FE_ALL_EXCEPT;
    unsigned int old_excepts;   // all previous masks

    if (fegetenv(&fenv))
    {
        return -1;
    }

    old_excepts = fenv.__control & FE_ALL_EXCEPT;

    // mask
    fenv.__control |= new_excepts;
    fenv.__mxcsr   |= new_excepts << 7;

    return fesetenv(&fenv) ? -1 : old_excepts;
}

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif  // darwin
#endif

// ************************************************************************* //
