AC_INIT([ckfilesystem],[0.1])
AM_INIT_AUTOMAKE([-Wall -Werror])
LT_INIT

# These compilation flags must be set before AC_PROG_CC/CXX,
# or we'll get "-g -O2" by default.
CFLAGS="${CFLAGS=} -g3 -O2 -Wall"
CXXFLAGS="${CXXFLAGS=} -g3 -O2 -Wall"

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

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

