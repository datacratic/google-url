AC_PREREQ([2.68])
AC_INIT([googleurl], [1.0.0])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign -Wall -Werror])

LT_INIT

AC_PROG_CXX

AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])],[],[AC_MSG_ERROR([No C++ compiler found])])
AC_LANG_POP([C++])

AC_LANG([C++])

AX_CHECK_ICU(4.8)

AC_CHECK_LIB([pthread], [pthread_create],[], [AC_MSG_ERROR([library 'pthread' is required])])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
