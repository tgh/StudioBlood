dnl Derived from
dnl
dnl Autoconf checks for LADSPA
dnl Daniel Kobras <kobras@linux.de>
dnl
dnl ACG_PATH_LADSPA([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl
dnl Enables switch --with-ladspa-dir=DIR.
dnl Substitutes LADSPA_CFLAGS as appropriate.
dnl Exports AM_CONDITIONAL called LADSPA.
AC_DEFUN([ACG_PATH_LADSPA],
[
AC_ARG_WITH(ladspa-dir, [ --with-ladspa-dir=DIR Directory where LADSPA header is installed],
ladspa_dir="$withval", ladspa_dir="")
if test "$ladspa_dir" != ""; then
LADSPA_CFLAGS="-I$ladspa_dir"
ac_save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $LADSPA_CFLAGS"
fi
AC_CHECK_HEADER(ladspa.h, ac_have_ladspa=yes, ac_have_ladspa=no)
if test "$ladspa_dir" != ""; then
CPPFLAGS="$ac_save_CPPFLAGS"
fi
if test "$ac_have_ladspa" = "yes"; then
ifelse([$1], , :, [$1])
else
LADSPA_CFLAGS=""
ifelse([$2], , :, [$2])
fi
AC_SUBST(LADSPA_CFLAGS)
AM_CONDITIONAL(LADSPA, test "$ac_have_ladspa" = "yes")
 
AC_ARG_WITH(ladspa-plugins, [ --with-ladspa-plugins=DIR Directory where LADSPA plugins should be installed],
ladspa_plugins="$withval", ladspa_plugins="")
if test "$ladspa_plugins" != ""; then
LADSPA_PLUGINS="$ladspa_plugins"
elif test "$prefix" != "NONE"; then
LADSPA_PLUGINS="$prefix/lib/ladspa"
else
LADSPA_PLUGINS="/usr/lib/ladspa"
fi
AC_SUBST(LADSPA_PLUGINS)
])
