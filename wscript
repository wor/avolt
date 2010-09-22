# -*- coding: utf-8 -*- vim:fenc=utf-8:ft=python:et:sw=4:ts=4:sts=4
# can be build easily with:
# clang $(pkg-config --cflags --libs alsa) -std=c99 avolt.c -o avolt
VERSION='0.0.1'
APPNAME='avolt'

top = '.'
out = '/tmp/avolt_build'


def set_options(opt):
        opt.add_option('--use-gcc', action='store_true', default=False, help='Prefer gcc as compiler.')
        opt.tool_options('compiler_cc')


def configure(conf):
        import Options
        print('→ the value of use-gcc is %r' % Options.options.use_gcc)
        print('→ configuring the project')

        conf.check_tool('compiler_cc')

        # enable clang if found
        conf.find_program('clang', var='CLANG_PATH', mandatory=False)
        if conf.env.CLANG_PATH:
            conf.env.CC = conf.env.CLANG_PATH
            conf.env.LINK_CC = conf.env.CLANG_PATH
            conf.env.CC_NAME = 'clang'
            conf.env.COMPILER_CC = 'clang'
            conf.env.CPP = conf.env.CLANG_PATH + "++"

        conf.check_cfg(path='pkg-config', args='--cflags --libs alsa',
                package='', uselib_store='MAIN')

        conf.env.CCDEFINES_MAIN = ['MAIN']
        conf.env.CCFLAGS_MAIN   = ['-mtune=core2', '-march=core2', '-O3', '-Wall', '-Wextra', '-fwhole-program', '-std=c99']
        conf.env.LIBPATH_MAIN   = ['/usr/lib']
        #print(conf.env)


def build(bld):
        t = bld(
                features     = ['cc', 'cprogram'],
                #source       = 'avolt.c',
                source       = bld.path.ant_glob('*.c'),
                install_path = '${SOME_PATH}/bin',
                target       = APPNAME,
                vnum         = VERSION,
                includes     = ['.'],
                #defines      = ['LINUX=1', 'BIDULE'],
                #ccflags      = ['-mtune=core2', '-march=core2', '-O3', '-Wall', '-Wextra', '-fwhole-program', '-std=c99'],
                userlib      = ['/usr/include/alsa'],
                libpath      = ['/usr/lib'],
                #lib          = ['asound'],
                #linkflags    = ['-g'],
                uselib      = 'MAIN'
        )
        t.rpath          = ['/usr/lib']
