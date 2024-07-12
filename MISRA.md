# MISRA Compliance

The CANopenNode files conform to the [MISRA C:2012](https://www.misra.org.uk)
guidelines, with some noted exceptions. Compliance is checked with [PC Lint Plus](https://pclintplus.com/).

A recommendation for MISRA: memory allocation and deallocation functions should not be used.
You must define the macro CO_USE_GLOBALS in your driver configuration.


## Configuration Inhibits Control

### Inhibits: Excluded the OD.c and OD.h files from the check because there are configuration parameters (not source code execution)
```
-efile( *, CANopenNode\OD.c )
-efile( *, CANopenNode\OD.h )
```

### Inhibits: Excluded the CO_gateway_ascii and CO_fifo (currently static analysis not completed)
```
-efile( *, CANopenNode\309\CO_gateway_ascii.c )
-efile( *, CANopenNode\309\CO_gateway_ascii.h )
-efile( *, CANopenNode\301\CO_fifo.c )
-efile( *, CANopenNode\301\CO_fifo.h )
```

### Inhibits: C comment contains '://' sequence
ref.: MISRA C 2012 Rule 3.1
```
-efile( 9259, CANopenNode* )
```

### Inhibits: unknown preprocessor directive 'string' in conditionally excluded region
ref.: MISRA C 2012 Rule 20.13
```
-efile( 9160, CANopenNode* )
```

### Inhibits: conversion from pointer to void to other pointer type (type)
ref.: MISRA C 2012 Rule 11.5
```
-efile( 9079, CANopenNode* )
```

### Inhibits: C comment contains C++ comment
ref.: MISRA C 2012 Rule 3.1
```
-efile( 9059, CANopenNode* )
```

### Inhibits: complete definition of symbol is unnecessary in this translation unit
ref.: MISRA C 2012 Dir 4.8
```
-efile( 9045, CANopenNode* )
```

### Inhibits: function parameter symbol modified
ref.: MISRA C 2012 Rule 17.8
```
-efile( 9044, CANopenNode* )
```

### Inhibits: cannot cast essential-type value to essential-type type
ref.: MISRA C 2012 Rule 10.5
```
-efile( 9030, CANopenNode* )
```

### Inhibits: function-like macro, 'macro', defined
ref.: MISRA C 2012 Dir 4.9
```
-efile( 9026, CANopenNode* )
```

### Inhibits: pasting/stringize operator used in definition of object-like/function-like macro 'string'
ref.: MISRA C 2012 Rule 20.10
```
-efile( 9024, CANopenNode* )
```

### Inhibits: performing pointer arithmetic via addition/subtraction
ref.: MISRA C 2012 Rule 18.4
```
-efile( 9016, CANopenNode* )
```

### Inhibits: local variable symbol could be pointer to const
ref.: MISRA C 2012 Rule 8.13
```
-efile( 954, CANopenNode* )
```

### Inhibits: return statement before end of function symbol
ref.: MISRA C 2012 Rule 15.5
```
-efile( 904, CANopenNode* )
```

### Inhibits: the left/right operand to operator always evaluates to 0
ref.: MISRA C 2004 Rule 13.7
```
-efile( 845, CANopenNode* )
```

### Inhibits: previous value assigned to symbol not used
```
-efile( 838, CANopenNode* )
```

### Inhibits: zero given as string argument to operator context
```
-efile( 835, CANopenNode* )
```

### Inhibits: parameter symbol of function symbol could be pointer to const
ref.: MISRA C 2012 Rule 8.13
```
-efile( 818, CANopenNode* )
```

### Inhibits: constant expression evaluates to 0 in 'unary/binary' operation 'operator'
```
-efile( 778, CANopenNode* )
```

### Inhibits: boolean condition for 'detail' always evaluates to 'detail'
ref.: MISRA C 2012 Rule 2.2 and Rule 14.3
```
-efile( 774, CANopenNode* )
```

### Inhibits: local macro 'string' not referenced
ref.:  MISRA C 2012 Rule 2.5
```
-efile( 750, CANopenNode* )
```

### Inhibits: constant value used in Boolean context (string)
```
-efile( 506, CANopenNode* )
```
