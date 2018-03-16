AC_PREREQ([2.69])
AC_INIT([memtool], [2018.03.0], [oss-tools@pengutronix.de])
AC_CONFIG_SRCDIR([memtool.c])
AM_INIT_AUTOMAKE([foreign dist-xz])

AC_PROG_CC
AC_PROG_MAKE_SET

AC_SYS_LARGEFILE

AC_ARG_ENABLE([mdio], [AS_HELP_STRING([--enable-mdio], [enable mdio access method @<:@default=check@:>@])],, [enable_mdio=check])

AS_IF([test "x$enable_mdio" != "xno"],
      [AC_CHECK_HEADERS([sys/socket.h linux/if.h linux/mii.h linux/sockios.h],,
			[AS_IF([test "x$enable_mdio" != "xyes"], [enable_mdio=no; break], [AC_MSG_ERROR([mdio depends on Linux mdio headers])])],
			dnl some versions of linux/if.h are not self-contained and need struct sockaddr
			dnl see https://git.kernel.org/linus/2618be7dccf8739b89e1906b64bd8d551af351e6
			[[#ifdef HAVE_SYS_SOCKET_H
			#  include <sys/socket.h>
			#endif
			]])
       AS_IF([test "x$enable_mdio" = "xcheck"], [enable_mdio=yes])
      ])

AC_MSG_CHECKING([for mdio support])
AS_IF([test "x$enable_mdio" = "xyes"],
      [AC_DEFINE([USE_MDIO], [1], [Define if the mdio access method should be built-in])
       AC_MSG_RESULT([yes])
      ], [AC_MSG_RESULT([no])])

AM_CONDITIONAL([MDIO], [test "x$enable_mdio" = "xyes"])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
