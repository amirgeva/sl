#extern fun gpu_block(array byte buffer)
#extern fun input_empty()
#extern fun input_read()

const W 11
const H 16
const AREA 171 # 11*16
const L 7

fun pixel_cursor(word x, word y)
	var array 6 byte cmd
	cmd[0] = 5
	cmd[1] = 5
	cmd[2] = lowbyte(x)
	cmd[3] = highbyte(x)
	cmd[4] = lowbyte(y)
	cmd[5] = highbyte(y)
	gpu_block(cmd)
end

fun draw_cube(word x, word y, byte color)
	var array 3 byte cmd
	pixel_cursor(x,y)
	cmd[0]=2
	cmd[1]=41
	cmd[2]=color
	gpu_block(cmd)
end

fun init_cubes()
	# Since we need to send 256 color bytes,
	# 2 command bytes and 2 length bytes
	# We'll split it into 2 buffers
	var array 5 byte header
	header[0]=4
	header[1]=40 # set sprite command
	var array 256 byte color
	color[0]=254
	var byte i
	i=0
	while i<64
		header[2]=i # sprite id
		header[3]=i # sprite color
		header[4]=i # sprite color
		var byte j
		j=0
		while j<254
			color[1+j]=i
			j=j+1
		end
		gpu_block(header)
		gpu_block(color)
		i=i+1
	end
end

fun wait_key()
	var byte b
	b = input_empty()
	if b>0
		return 255
	end
	b = input_read()
	return b
end

struct Board
	var array AREA byte grid
	var byte game_over
end

struct Piece
	var byte cx
	var byte cy
	var array 8 byte offsets
	var byte color
	var byte valid
end


fun init_board(Board board)
	var byte i
	i=0
	while i<AREA
		board.grid[i] = 0
		i=i+1
	end
	board.game_over=0
end

fun board_index(byte x, byte y)
	if x >= W
		return 255
	end
	if y >= H
		return 255
	end
	var byte index
	#index = multiply(y,W)
	index = (y<<3)+(y<<1)+y  # Same as y*11
	index = index + x
	return index
end

fun pos_free(Board board, byte x, byte y)
	var byte index
	index = board_index(x,y)
	if index=255
		return 0
	end
	if board.grid[index] = 0
		return 1
	end
	return 0
end


fun main()
	var Board board
	init_board(board)
	init_cubes()
end
