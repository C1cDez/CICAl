# CICAL

CICAL (C Interpretable Calculator) is a console calculator
capable of handling complex algebraic expressions,
dynamic variable reassignment, and user-defined functions evaluation.

---

## Installation

### Building
For **Windows**:
```bat
gcc src/*.c -o cical.exe -Wall -Wextra
```

For **Linux**:
```bash
gcc src/*.c -o cical -lm -Wall -Wextra
```

### Running
```bash
cical
```

Or *(for instant calculations)*:
```bash
cical expr1 ; expr2 ; ... 
```
Expressions can be written without quotes (`'` or `"`),
but must be separated by semicolons `;`.

---

## User manual

This manual describes end-user interaction with CICAL.
You can also read this manual inside the program using the `!m` control command.

**Basic arithmetic operations** are the following:
`+` (addition), `-` (subtraction), `*` (multiplication), and `/` (division).
CICAL also uses `^` as exponentiation, and `|x|` to take absolute value.
Multiplication can be written implicitly (without sign) between anything,
but this prioritizes it over division and regular explicit multiplication.

**Numbers** fall into two categories:
1. *Doubles* are any number with a fractional part. They are written with decimal separator `.`.
2. *Bigints* (arbitrary sized integers), as seen by their name, can have any number of digits without loss in precision.

*Note:* Bigints are automatically converted into double if one of the operands is double.
This causes **huge** precision loss if you are working with big numbers.

**Variables** are the same thing as they are in algebra.
You can use any latin symbol [a-zA-Z] ot certain greek letters to name a variable.
Variable can have a subscript (or index), which is just a number written after it.
To create a variable, name it, put `=` sign and define its value.

**Greek letters** are the following words, which are treated as full-fledged units:
`alpha`, `beta`, `gamma`, `zeta`, `mu`, `pi`, `rho`, `tau`, `phi`, `psi`.

**Constants** are predefined and immutable variables borrowed directly from math:
`pi`, `e`, `tau` *(= 2pi)*, `phi` *(golden ratio const.)*, `gamma` *(Euler Maschroni const.)*.

**Functions:**
* General: `sqrt`, `cbrt`, `ln` *(log base e)*, `lg` *(log base 10)*, `log`, `exp`, `erf` *(error function)*.
* Trigonometry: `[arc]sin[h]`, `[arc]cos[h]`, `[arc]tan[h]`, `[arc]cot[h]`.
* Number theory: `gcd`, `lcm`, `mod`, `pow`, `inv`.
* Misc: `min`, `max`.

*Note:* number theory functions can be applied only to *bigints*.

You can define custom function by naming it, enumerating its arguments in parenthesis, placing `=`
and defining it after. If ambiguity between function and variable occurs,
preference is given to a function.

**Previous result** *(or `Ans` on real calculator)* is referenced by `@` sign.

**Control commands** are simple shortcuts to control calculator behaviour.
* `!h` - Lists control commands.
* `!c` - Clears the screen.
* `!q` - Quits the calulator.
* `!m` - Prints manual.
* `!u[f|v]` - Undefines all functions and variables.
* `!s[f|v]` - Shows defined functions and variables.

---

## Interpreting pipeline

```
                 "INPUT LINE"
                      |
                      V
                 +---------+     Produces stream of tokens,
      /--------> |  LEXER  |     objects parser can easily work with,
      |          +---------+     from given string.
      |               |
    -----             V
   /     \      +----------+     Analyzes given stream of tokens
  | CORE* | --> |  PARSER  |     and constructs Abstract Syntax Tree
   \     /      +----------+     to represent hierarchical structure
    -----             |
      |               V
      |          +----------+    Recursively goes through AST,
      \--------> | COMPUTER |    computes numbers, resolves
                 +----------+    variables and functions.
                      |
                      V
                  (res, err)

     *CORE stores all the variables and functions for the current seesion.
```
