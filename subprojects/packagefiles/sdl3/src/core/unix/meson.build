sources += files(
  'SDL_appid.c',
  'SDL_fribidi.c',
  'SDL_gtk.c',
  'SDL_libthai.c',
  'SDL_poll.c',
)

have_unix_usergroupids_code = '''
    // required for GNU\Linux feature_test_macros(7)
    #define _GNU_SOURCE
    #include <unistd.h>
    int main(int argc, char** argv) { 
	  getresgid; 
	  getresuid; 
	}
'''
have_unix_usergroupids = cc.compiles(
  have_unix_usergroupids_code,
  name: 'have unix effective User/Group functions support',
)

if have_unix_usergroupids
  cdata.set('HAVE_GETRESGID', 1)
  cdata.set('HAVE_GETRESUID', 1)
endif


