set nocompatible	" Use Vim defaults instead of 100% vi compatibility
set backspace=indent,eol,start	" more powerful backspacing
set textwidth=0		" Don't wrap lines by default
set nobackup
set history=50		" keep 50 lines of command line history
set ruler		" show the cursor position all the time

set t_Co=256
set t_Sf=[3%dm
set t_Sb=[4%dm

function Scroll(dir, windiv)
	let wh = winheight(0)
	let i = 1
	while i < wh / a:windiv
		let i = i + 1
		if a:dir == "d"
			normal j
		else
			normal k
		end
		" insert a character to force vim to update!
		normal I 
		redraw
		normal dl
	endwhile
endfunction

function WindowScroll(dir, windiv)
	let wh = winheight(0)
	let i = 1
	while i < wh * a:windiv
		let i = i + 1
		if a:dir == "d"
			normal j
		else
			normal k
		end
		" insert a character to force vim to update!
		normal I 
		redraw
		normal dl
	endwhile
endfunction

function AutoScroll(count)
	let loop = 0
	while loop < a:count
		let loop = loop + 1
		call Scroll("d", 1)
		call Scroll("u", 2)
		call Scroll("d", 2)
		call Scroll("u", 1)
		call Scroll("d", 2)
		call Scroll("u", 2)
	endwhile
	quit!
endfunction

function AutoWindowScroll(count)
	let loop = 0
	while loop < a:count
		let loop = loop + 1
		call WindowScroll("d", 10)
		call WindowScroll("u", 10)
	endwhile
	quit!
endfunction
