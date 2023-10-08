# "Simple Language"
A programming language designed for compilation in limited 64k retro machines.
The implementation of the compiler outputs z80 machine code.

The simplicity intended is to make a compact compiler possible, not necessarily to make programming with it easier.
The language is designed to have no ambiguities, and have meaningful parsing tokens appear earlier in the code.
Also, since each statement has to be in its own line, there's no need for semicolons.
For exmaple, when declaring an array, your specify that it's an array explicitely, followed by the size, followed
by the element type and finally the variable name.
```
var array 12 byte myarray

vs.

byte myarray[12];
```
Note that until it encounters the left bracket, a parser of C cannot know it's an array.


Current binary size of the compiler for a z80 is around 28KB  
and requires several KBs of RAM for compiliing a typical source code file.

There is still no integration with disk io to read files, so it is tested on a windows machine.

Basic language constructs include:
1. Primitive variables of type byte and word
2. Arrays of existing types
3. Structs of fields on existing types
4. Functions (procedural programming)
  1. Assignment statements
  2. If / else statements
  3. While statements


Example program for computing a fibonacci number and printing it to the screen

```
fun fib(byte x)
	var byte p
	var byte q
	if x<2
		return 1
	else
		p=fib(x-1)
		q=fib(x-2)
		return p+q
	end
end

fun main()
	var byte a
	a=fib(7)
end
```
