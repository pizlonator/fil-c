strmbase_sources = [
  'amextra.cpp',
  'amfilter.cpp',
  'amvideo.cpp',
  'arithutil.cpp',
  'combase.cpp',
  'cprop.cpp',
  'ctlutil.cpp',
  'ddmm.cpp',
  'dllentry.cpp',
  'dllsetup.cpp',
  'mtype.cpp',
  'outputq.cpp',
  'perflog.cpp',
  'pstream.cpp',
  'pullpin.cpp',
  'refclock.cpp',
  'renbase.cpp',
  'schedule.cpp',
  'seekpt.cpp',
  'source.cpp',
  'strmctl.cpp',
  'sysclock.cpp',
  'transfrm.cpp',
  'transip.cpp',
  'videoctl.cpp',
  'vtrans.cpp',
  'winctrl.cpp',
  'winutil.cpp',
  'wxdebug.cpp',
  'wxlist.cpp',
  'wxutil.cpp'
]

strmbase_cpp_args = ['-D_MBCS']
strmbase_cpp_args += cxx.get_supported_arguments([
  '/wd4189', # 'identifier' : local variable is initialized but not referenced
  '/wd4456', # declaration of 'identifier' hides previous local declaration
  '/wd4701', # potentially uninitialized local variable 'name' used
  '/wd4703', # potentially uninitialized local pointer variable 'name' used
  '/wd4706', # assignment within conditional expression
  '/wd4996'  # code uses a function, class member, variable, or typedef that's marked deprecated
])

strmbase_subdir = 'baseclasses'
strmbase_includes = include_directories(strmbase_subdir)

strmiids_dep = cxx.find_library('strmiids', required: get_option('directshow'))
if not strmiids_dep.found()
  message('strmiids not found, not building directshow strmbase')
  strmbase_dep = disabler()
  subdir_done()
endif

strmbase_cpp_sources = []
foreach file : strmbase_sources
  strmbase_cpp_sources += strmbase_subdir + '/' + file
endforeach

strmbase_lib = static_library(
  'strmbase',
  strmbase_cpp_sources,
  cpp_args: strmbase_cpp_args,
  dependencies: strmiids_dep,
  include_directories: strmbase_includes,
  override_options: ['cpp_std=none'])

strmbase_dep = declare_dependency(
  link_with: strmbase_lib,
  compile_args: strmbase_cpp_args,
  dependencies: strmiids_dep,
  include_directories: strmbase_includes)
