# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.69])
dnl TODO: Bug-report address
AC_INIT([Fachoda Complex], [[2.2]], , ,
        [[http://rixed.github.com/fachoda-complex/]])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([[ravages.c]])

AC_PROG_CC
AC_PROG_CC_C99
PKG_PROG_PKG_CONFIG

AC_CHECK_LIB([[m]], [[sqrt]], ,
             [AC_MSG_ERROR([[cannot find math library (libm)]])])
AM_PATH_SDL
PKG_CHECK_MODULES([OPENAL], [[openal]])
dnl libjpeg likes to define lots of its functions as macros depending
dnl on its whim and the capabilities of the linker, so it's hard to
dnl tell what the names of the actual functions are...
AC_CHECK_LIB([[jpeg]], [[jpeg_std_error]], ,
             [AC_CHECK_LIB([[jpeg]], [[jStdError]], ,
                           [AC_MSG_ERROR([[cannot find libjpeg]])])])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
