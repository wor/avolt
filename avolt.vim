let SessionLoad = 1
if &cp | set nocp | endif
let s:so_save = &so | let s:siso_save = &siso | set so=0 siso=0
let v:this_session=expand("<sfile>:p")
silent only
cd /home/repo/avolt
if expand('%') == '' && !&modified && line('$') <= 1 && getline(1) == ''
  let s:wipebuf = bufnr('%')
endif
set shortmess=aoO
badd +3 avolt.conf
badd +44 avolt.conf.h
badd +10 avolt.conf.c
badd +26 avolt.c
badd +5 volume_change.h
badd +187 volume_change.c
badd +1 wutil.h
badd +1 wutil.c
badd +1 volume_mapping.h
badd +16 volume_mapping.c
badd +27 cmdline_options.h
badd +60 cmdline_options.c
badd +12 alsa_utils.h
badd +8 alsa_utils.c
args avolt.c
edit avolt.conf
set splitbelow splitright
set nosplitbelow
set nosplitright
wincmd t
set winheight=1 winwidth=1
argglobal
setlocal fdm=syntax
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
3
normal zo
11
normal zo
19
normal zo
11
normal zo
42
normal zo
62
normal zo
69
normal zo
let s:l = 19 - ((18 * winheight(0) + 38) / 76)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
19
normal! 036l
tabnext 1
if exists('s:wipebuf')
  silent exe 'bwipe ' . s:wipebuf
endif
unlet! s:wipebuf
set winheight=1 winwidth=20 shortmess=filnxtToO
let s:sx = expand("<sfile>:p:r")."x.vim"
if file_readable(s:sx)
  exe "source " . fnameescape(s:sx)
endif
let &so = s:so_save | let &siso = s:siso_save
doautoall SessionLoadPost
unlet SessionLoad
" vim: set ft=vim :
