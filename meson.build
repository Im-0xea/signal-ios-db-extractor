project('signal-ios-db-extractor', 'c')

inc = [
  'include'
]
src = [
  'src/html.c',
  'src/irc.c',
  'src/seqdump.c'
]
deps = [
  dependency('libplist', version : '>=2.3.0'),
  dependency('sqlite3')
]
executable('seqdump',
  sources : src,
  dependencies : deps,
  include_directories : inc
)
