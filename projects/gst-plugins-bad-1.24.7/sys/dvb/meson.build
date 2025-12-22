dvb_sources = [
  'camapplication.c',
  'camapplicationinfo.c',
  'cam.c',
  'camconditionalaccess.c',
  'camdevice.c',
  'camresourcemanager.c',
  'camsession.c',
  'camswclient.c',
  'camtransport.c',
  'camutils.c',
  'dvbbasebin.c',
  'gstdvb.c',
  'gstdvbelement.c',
  'gstdvbsrc.c',
  'parsechannels.c',
]

if get_option('dvb').disabled()
  subdir_done()
endif

dvb_check_code = '''
#include <linux/dvb/version.h>
#if DVB_API_VERSION < 5 || (DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR <= 4)
#error "Incompatible dvb API"
#endif'
'''
have_dvb = cc.compiles(dvb_check_code)
if not have_dvb and get_option('dvb').enabled()
  error('DVB plugin enabled but dvb headers not found')
endif

if have_dvb
  gstdvb = library('gstdvb',
    dvb_sources,
    c_args : gst_plugins_bad_args + [ '-DGST_USE_UNSTABLE_API' ],
    include_directories : [configinc, libsinc],
    dependencies : [gstmpegts_dep, gstbase_dep],
    install : true,
    install_dir : plugins_install_dir,
  )
  plugins += [gstdvb]
endif
