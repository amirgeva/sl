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
