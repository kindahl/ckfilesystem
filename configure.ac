AC_INIT
AM_INIT_AUTOMAKE(ckfilesystem,0.1)
AM_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

AC_CONFIG_MACRO_DIR([m4])

# Version information (current:revision:age).
CKFILESYSTEM_VERSION=1:0:0
AC_SUBST(CKFILESYSTEM_VERSION)

AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

# With arguments.
AC_ARG_WITH([ckcore],
            [AS_HELP_STRING([--with-ckcore],
         				    [installation prefix of ckcore])],
            [with_ckcore=$withval],
            [with_ckcore=no])

if test -d "$with_ckcore"
then
	CXXFLAGS="$CXXFLAGS -I$with_ckcore/include"
	LIBS="$LIBS -L$with_ckcore/lib -lckcore"
fi

AS_IF([test "x$with_ckcore" != xno],
	  [AC_CHECK_LIB([ckcore],[main],[],
              		[AC_MSG_FAILURE([--with-ckcore was given, but test for ckcore failed])])])

AC_DEFINE(_UNIX)
AC_DEFINE(_FILE_OFFSET_BIT,64,[Enable support for large files.])

AC_OUTPUT(Makefile src/Makefile)

