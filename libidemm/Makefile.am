## Copyright (C) 2016 Marcin Kolny <marcin.kolny@gmail.com>
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.

if ENABLE_IDEMM

srcmm_subdirs = idemm
idemm_include_HEADERS = idemm.h

if MAINTAINER_MODE
src_subdirs = src
else
src_subdirs =
endif

# TODO 
# if ENABLE_DOCUMENTATION
# doc_subdirs = docs
# else
# doc_subdirs =
# endif

SUBDIRS = tools $(src_subdirs) $(srcmm_subdirs) # $(doc_subdirs)

idemm_includedir = $(includedir)/gnome-builder-@VERSION@/$(IDEMM_MODULE_NAME)
idemm_libincludedir = $(pkglibdir)/$(IDEMM_MODULE_NAME)/include
nodist_idemm_libinclude_HEADERS = idemmconfig.h

pkgconfigdir = $(pkglibdir)/pkgconfig
pkgconfig_DATA = $(pkgconfig_files)

endif
