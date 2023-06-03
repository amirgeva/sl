
struct A
	var array 8 byte arr
	var word val
end

fun process(A a)
	a.arr[3]=5
	a.val=4096
end


fun main()
	var A a
	process(a)
end
