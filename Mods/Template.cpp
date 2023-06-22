#include <Mods.h>

/*
    Code here must be position independent because it will be copied to the game process.
    A non-exhaustive list of things that violate this:
    - Using string constants or global variables
    - Referencing a symbol not defined in AE.h
    - Casting to/from floating-point
    - Most C++ constructs (exceptions, new/delete, etc)
*/




END_INJECT_CODE;

// Your Init function should be the only function declared below END_INJECT_CODE

void InitTemplate() // <-- replace "Template" with the name of your mod
{
    // Call functions declared in Mods.h here to instantiate your mod.
    // Reference the other mods for example usage.
}