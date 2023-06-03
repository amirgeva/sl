# Sum 1..n
fun sum_series(byte n)
	var byte sum
	var byte i
	sum=0
	i=1
	while i<=n
		sum=sum+i
		i=i+1
	end
	return sum
end

fun main()
	var byte a
	a=sum_series(5)
end
