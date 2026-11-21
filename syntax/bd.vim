
" boook syntax highlighting
if exists("b:current_syntax")
  finish
endif

" Headings, filenames, and sections
syntax match bdHeading /^# .*$/
syntax match bdFilename /^! .*$/
syntax match bdSection /^: .*$/

" Inline formatting
syntax region bdBold start=/\*/ end=/\*/ oneline
syntax region bdItalic start=/\// end=/\// oneline
syntax region bdUnderline start=/_/ end=/_/ oneline

" Links: [label](filename.bd)
syntax match bdLink /\[[^]]\+\]([^)]\+)/ contains=bdLinkTarget
syntax match bdLinkTarget /([^)]\+)/ contained

" Colors
highlight default link bdHeading Title
highlight default link bdFilename PreProc
highlight default link bdSection Statement
highlight default link bdBold Bold
highlight default link bdItalic Italic
highlight default link bdUnderline Underlined
highlight default link bdLink Underlined
highlight default link bdLinkTarget String

let b:current_syntax = "bd"

