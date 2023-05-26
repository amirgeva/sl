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


Current binary size of the compiler for a z80 is around 22KB  
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
var array 16 byte hex

fun init_hex()
	var byte i
	i=0
	while i<16
		if i<10
			hex[i]=i+48
		else
			hex[i]=i+55
		end
		i=i+1
	end
end

fun printnum(byte num)
	var byte upper
	var byte lower
	upper = num >> 4
	lower = num & 15
	var array 6 byte buffer
	buffer[0]=5
	buffer[1]=30
	buffer[2]=2
	buffer[3]=hex[upper]
	buffer[4]=hex[lower]
	buffer[5]=4
	gpu_block(buffer)
end


fun fib(byte x, array byte res)
	var byte p
	var byte q
	if (x=0) | (x=1)
		res[0]=1
	else
		fib(x-1,p)
		fib(x-2,q)
		res[0]=p+q
	end
end

fun main()
	var array 1 byte a
	init_hex()
	fib(7,a)
	printnum(a[0])
end
```
