struct A
	var byte val
	var byte size
end
fun calc(word x)
	var array 8 A a
	if a[3].val<5
		a[2].size = x+1
	end
end
fun main()
	calc(5)
end