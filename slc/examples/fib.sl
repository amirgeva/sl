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
	init_hex()
	a=fib(7)
	printnum(a)
end
