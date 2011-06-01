# -*- coding: utf-8 -*- vim:fenc=utf-8:ft=python:et:sw=4:ts=4:sts=4
# can be build easily with:
# clang $(pkg-config --cflags --libs alsa) -std=c99 avolt.c -o avolt
VERSION='0.0.1'
APPNAME='avolt'

top = '.'
out = "/tmp/" + APPNAME + "_build"


def options(opt):
        opt.add_option(
                '--use-gcc',
                action='store_true',
                default=False,
                help='Prefer gcc as compiler.')
        opt.tool_options('compiler_c')


def configure(conf):
        import Options

        conf.check_tool('compiler_c')

        # find programs
        conf.find_program('clang', var='CLANG_PATH', mandatory=False)

        # enable clang if found and if not --use-gcc option given
        if conf.env.CLANG_PATH and not Options.options.use_gcc:
            conf.env.CC = conf.env.CLANG_PATH
            conf.env.LINK_CC = conf.env.CLANG_PATH
            conf.env.CC_NAME = 'clang'
            conf.env.COMPILER_CC = 'clang'
            # conf.env.CPP = conf.env.CLANG_PATH + "++"

        conf.check_cfg(
                package='alsa',
                args='--cflags --libs',
                uselib_store='MAIN')

        conf.env.CCDEFINES_MAIN = ['MAIN']
        conf.env.CCFLAGS_MAIN   = ['-mtune=core2', '-march=core2', '-O3', '-Wall', '-Wextra', '-fwhole-program', '-std=c99']
        #conf.env.LIBPATH_MAIN   = ['/usr/lib']

        # Add additional system libraries for linking
        conf.env.LIB_MAIN.append('pthread') # rt is works for gcc but not clang??


def build(bld):
        t = bld(
                features     = ['c', 'cprogram'],
                source       = bld.path.ant_glob('*.c'),
                install_path = '${SOME_PATH}/bin',
                target       = APPNAME,
                vnum         = VERSION,
                includes     = ['.'],
                #libpath      = ['/usr/lib'],
                uselib      = 'MAIN'
        )
        t.rpath          = ['/usr/lib']
