
var array 56 byte offsets = [
	254,   0, 255,   0,   0,   0,   1,   0,
	255,   0,   0,   0,   1,   0,   1,   1,
	255,   0,   0,   0,   1,   0,   1, 255,
	255,   0,   0,   0,   1,   0,   0,   1,
	255,   0,   0,   0,   0, 255,   1, 255,
	255,   0,   0,   0,   0,   1,   1,   1,
	255,   0,   0,   0, 255,   1,   0,   1
]


const W 11
const H 16
const AREA 176 # 11*16
const L 7


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

extern fun move_piece(Board board, Piece piece, byte dx, byte dy)

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
			j=j+1
			color[j]=i
		end
		gpu_block(header)
		gpu_block(color)
		i=i+1
	end
end

fun wait_key()
	if input_empty()>0
		return 255
	end
	return input_read()
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

fun remove_full(Board board)
	var byte full_rows
	var byte ofs
	var byte y
	var byte count
	var byte x
	full_rows=0
	ofs = 161  # (H-1)*W
	y=H-1
	while y>0
		count=0
		x=0
		while x<W
			if board.grid[ofs+x]>0
				count=count+1
			end
			x=x+1
		end
		if count=W
			full_rows=full_rows+1
			var byte aofs
			var byte ay
			aofs = ofs
			ay = y
			while ay>0
				x=0
				while x<W
					board.grid[aofs+x]=board.grid[aofs+x-W]
					x=x+1
				end
				aofs=aofs-W
				ay=ay-1
			end
			x=0
			while x<W
				board.grid[x]=0	# Clear first row
				x=x+1
			end
			y=y+1				# Repeat same row
			ofs = ofs + W		#
		end
		y=y-1
		ofs=ofs-W
	end
	
	if full_rows>0
		var word wy
		var word wx
		ofs=0
		y=0
		while y<H
			wy=y
			wy = wy << 4
			x=0
			while x<W
				wx=x+L
				wx=wx<<4
				draw_cube(wx,wy,board.grid[ofs])
				x=x+1
				ofs=ofs+1
			end
			y=y+1
		end
	end
end

fun place_piece(Board board, Piece piece)
	var byte i
	var byte x
	var byte y
	var byte idx
	i=0
	while i<4
		x=piece.offsets[(i<<1)+0]+piece.cx
		y=piece.offsets[(i<<1)+1]+piece.cy
		idx=board_index(x,y)
		if idx!=255
			board.grid[idx]=piece.color
		else
			x=y
		end
		i=i+1
	end
	piece.valid=0
end

fun gen_piece(Board board, Piece piece)
	var byte i
	var byte type
	var byte base
	piece.cx=5
	piece.cy=2
	type = lowbyte(rng()) & 7
	if type<7
		base = type << 3
		i=0
		while i<8
			piece.offsets[i]=offsets[base+i]
			i=i+1
		end
		piece.color=type+1
		piece.valid=1
		if move_piece(board, piece, 0, 0)=0
			board.game_over=1
		end
		return 1
	end
	return 0
end

fun draw_piece(Piece piece, byte erase)
	var byte color
	var byte i
	var byte x
	var byte y
	var word wx
	var word wy
	color = 0
	if erase=0
		color=piece.color
	end
	i=0
	while i<4
		x=piece.cx+piece.offsets[(i<<1)+0]
		y=piece.cy+piece.offsets[(i<<1)+1]
		x=x+L
		wx=x
		wy=y
		wx=wx<<4
		wy=wy<<4
		draw_cube(wx,wy,color)
		i=i+1
	end
end

fun move_piece(Board board, Piece piece, byte dx, byte dy)
	var byte i
	var byte x
	var byte y
	i=0
	while i<4
		x=piece.cx+piece.offsets[(i<<1)+0]+dx
		y=piece.cy+piece.offsets[(i<<1)+1]+dy
		if pos_free(board, x, y)=0
			return 0
		end
		i=i+1
	end
	draw_piece(piece, 1)
	piece.cx=piece.cx+dx
	piece.cy=piece.cy+dy
	draw_piece(piece,0)
	return 1
end

fun rotate_piece(Board board, Piece piece)
	var array 8 byte roffsets
	var byte i
	var byte j
	var byte x
	var byte y
	j=0
	i=0
	while i<4
		roffsets[j]=0-piece.offsets[j+1]
		roffsets[j+1]=piece.offsets[j]
		x = roffsets[j] + piece.cx
		y = roffsets[j+1] + piece.cy
		if pos_free(board, x, y)=0
			return 0
		end
		i=i+1
		j=j+2
	end
	draw_piece(piece,1)
	i=0
	while i<8
		piece.offsets[i]=roffsets[i]
		i=i+1
	end
	draw_piece(piece,0)
	return 1
end


fun draw_frame()
	var word x0
	var word x1
	var byte x
	var byte y
	var word wy
	var word wx
	x0=96 # (L-1)*16
	x1=288 # (L+W)*16
	
	y=0
	wy=0
	while y<H
		draw_cube(x0,wy,63)
		draw_cube(x1,wy,63)
		wy=wy+16
		y=y+1
	end
	x=L-1
	wx=96   # (L-1)*16
	while x<=(L+W)
		draw_cube(wx,wy,63)
		wx=wx+16
		x=x+1
	end
end



fun main()
	var Board board
	var Piece piece
	var byte key
	init_cubes()
	cls()
	init_board(board)
	piece.valid=0
	draw_frame()
	var word last_timer
	last_timer=0
	while 1>0
		if piece.valid=0
			while gen_piece(board, piece)=0
			end
			if piece.valid>0
				draw_piece(piece, 0)
			end
			if board.game_over>0
				cls()
				return 0
			end
		end
		key = wait_key()
		if key!=255
			if key=16
				move_piece(board, piece, 255, 0)
			end
			if key=18
				move_piece(board, piece, 1, 0)
			end
			if key=19
				move_piece(board, piece, 0, 1)
			end
			if key=17
				rotate_piece(board, piece)
			end
		else
			var word cur_timer
			var word diff
			cur_timer=timer()
			diff = cur_timer - last_timer
			if diff>50
				last_timer=cur_timer
				if move_piece(board,piece,0,1)=0
					place_piece(board,piece)
					remove_full(board)
				end
			end
		end
	end
end
