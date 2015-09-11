#!/bin/bash
#
# generate.sh - Should we turn this into a Makefile?
# ========
#
# (C) Jean-Pierre Lozi, 2011
#

source "../inc/color-echo.sh"

BERKELEYDB_VERSION="5.2.28.NC"
PATCH_NAME="../cocci/bd.out"

if [[ $# -eq 1 ]]; then
	PATCH_NAME=$1
fi

##

cecho "#######################################################################"\
"##################" $ltred
cecho "# Liblock                                                              "\
"                 #" $ltred
cecho "#######################################################################"\
"##################" $ltred

##

cecho "Compiling the liblock (output in logs/liblock-make.log)..." $cyan
cd ../liblock/
make distclean > ../berkeleydb/logs/liblock-make.log
make >> ../berkeleydb/logs/liblock-make.lo
if [[ "$?" != "0" ]] ; then
    cecho "An error has occured: check the log. Exiting." $ltred
    exit
else
    cecho "The liblock was compiled successfully." $ltgreen
fi
cd ../berkeleydb/

##

cecho "#######################################################################"\
"##################" $ltred
cecho "# Berkeley DB - original                                               "\
"                 #" $ltred
cecho "#######################################################################"\
"##################" $ltred

cecho "Deleting db-$BERKELEYDB_VERSION and db-$BERKELEYDB_VERSION-original... " $cyan -n
rm -rf db-$BERKELEYDB_VERSION db-$BERKELEYDB_VERSION-original
cecho "done." $ltgreen
	
##
	
cecho "Extracting files/db-$BERKELEYDB_VERSION.tar.gz... " $cyan -n
if [[ ! -f files/db-$BERKELEYDB_VERSION.tar.gz ]]; then
	cecho "Couldn't find files/db-$BERKELEYDB_VERSION.tar.gz. Exiting." $ltred
	exit
fi
tar -xzf files/db-$BERKELEYDB_VERSION.tar.gz
sed "s/__atomic_compare_exchange/__atomic_compare_exchange2/" ./db-5.2.28.NC/src/dbinc/atomic.h > ./db-5.2.28.NC/src/dbinc/atomic2.h 
mv ./db-5.2.28.NC/src/dbinc/atomic2.h ./db-5.2.28.NC/src/dbinc/atomic.h
#rm -r ../bd-orig/db-$BERKELEYDB_VERSION
#cp -r db-$BERKELEYDB_VERSION ../bd-orig
cecho "done." $ltgreen

##

configure_make_berkeleydb()
{
	cecho "Running configure (output in logs/berkeleydb-configure.log)..." $cyan
	cd db-$BERKELEYDB_VERSION/build_unix/
	../dist/configure > ../../logs/berkeleydb-configure.log
	if [[ "$?" != "0" ]] ; then
		cecho "An error has occured: check the log. Exiting." $ltred
		exit
	else
		cecho "Berkeley DB was configured successfully." $ltgreen
	fi
	cd ../..

	##

	cecho "Replacing the Makefile... " $cyan -n
	if [[ ! -f files/$1 ]]; then
		cecho "Couldn't find files/$1. Exiting." $ltred
		exit
	fi
	cp files/$1 db-$BERKELEYDB_VERSION/build_unix/Makefile
	cecho "done." $ltgreen

	##

	cecho "Detecting the number of cores... " $cyan -n
	N_CORES=`grep -c ^processor /proc/cpuinfo`
	cecho "$N_CORES cores found." $ltgreen

	##

	cecho "Compiling Berkeley DB using "$((N_CORES+1))\
" threads (output in logs/berkeleydb-make.log)..." $cyan
	cd db-$BERKELEYDB_VERSION/build_unix/
	make -j $((N_CORES+1)) libdb.a > ../../logs/berkeleydb-make.log 
	if [[ "$?" != "0" ]] ; then
		cecho "An error has occured: check the log. Exiting." $ltred
		exit
	else
		cecho "Berkeley DB was compiled successfully." $ltgreen
	fi
	cd ../..
}

configure_make_berkeleydb Makefile-original

##

cecho "Moving db-$BERKELEYDB_VERSION to db-$BERKELEYDB_VERSION-original... " $cyan -n
mv db-$BERKELEYDB_VERSION db-$BERKELEYDB_VERSION-original
cecho "done." $ltgreen

##

cecho "#######################################################################"\
"##################" $ltred
cecho "# TPC - C w/ Berkeley DB original                                      "\
"                 #" $ltred
cecho "#######################################################################"\
"##################" $ltred

##

cecho "Deleting TPCC-BDB-RELEASE-original... " $cyan -n
rm -rf TPCC-BDB-RELEASE-original
cecho "done." $ltgreen

##

cecho "Copying TPCC-BDB-RELEASE-base to TPCC-BDB-RELEASE-original... " $cyan -n
cp -r TPCC-BDB-RELEASE-base TPCC-BDB-RELEASE-original
cecho "done." $ltgreen

##

cecho "Compiling TPC-C (output in logs/tpcc-original-make.log)..." $cyan
cd TPCC-BDB-RELEASE-original
sed "s/DBDIR=.*/DBDIR=..\/db-5.2.28.NC-original\/build_unix/" Makefile > Makefile-mod
mv Makefile-mod Makefile
make clean > ../logs/tpcc-berkeleydb-original-make.log
make >> ../logs/tpcc-berkeleydb-original-make.log
if [[ "$?" != "0" ]] ; then
	cecho "An error has occured: check the log. Exiting." $ltred
	exit
else
	cecho "TPC-C was compiled successfully." $ltgreen
fi
cd ..

##

cecho "#######################################################################"\
"##################" $ltred
cecho "# Berkeley DB - patched                                                "\
"                 #" $ltred
cecho "#######################################################################"\
"##################" $ltred

##

cecho "Deleting db-$BERKELEYDB_VERSION and db-$BERKELEYDB_VERSION-patched... " $cyan -n
rm -rf db-$BERKELEYDB_VERSION db-$BERKELEYDB_VERSION-patched
cecho "done." $ltgreen

##

cecho "Extracting files/db-$BERKELEYDB_VERSION.tar.gz... " $cyan -n
if [[ ! -f files/db-$BERKELEYDB_VERSION.tar.gz ]]; then
	cecho "Couldn't find files/db-$BERKELEYDB_VERSION.tar.gz. Exiting." $ltred
	exit
fi
tar -xzf files/db-$BERKELEYDB_VERSION.tar.gz
sed "s/__atomic_compare_exchange/__atomic_compare_exchange2/" ./db-5.2.28.NC/src/dbinc/atomic.h > ./db-5.2.28.NC/src/dbinc/atomic2.h
mv ./db-5.2.28.NC/src/dbinc/atomic2.h ./db-5.2.28.NC/src/dbinc/atomic.h
cecho "done." $ltgreen

##

cecho "Replacing dbreg.c with the modified version... " $cyan -n
if [[ ! -f ../bd-orig/db-5.2.28.NC/src/dbreg/dbreg.c ]]; then
	cecho "Couldn't find ../bd-orig/db-5.2.28.NC/src/dbreg/dbreg.c. Exiting." $ltred
	exit
fi
cp ../bd-orig/db-5.2.28.NC/src/dbreg/dbreg.c db-$BERKELEYDB_VERSION/src/dbreg/
cecho "done." $ltgreen

##

cecho "Replacing db.c with the modified version... " $cyan -n
if [[ ! -f ../bd-orig/db-5.2.28.NC/src/db/db.c ]]; then
	cecho "Couldn't find ../bd-orig/db-5.2.28.NC/src/db/db.c. Exiting." $ltred
	exit
fi
cp ../bd-orig/db-5.2.28.NC/src/db/db.c db-$BERKELEYDB_VERSION/src/db/
cecho "done." $ltgreen

##

cecho "Deleting TPCC-BDB-RELEASE... " $cyan -n
rm -rf TPCC-BDB-RELEASE
cecho "done." $ltgreen

##

cecho "Copying TPCC-BDB-RELEASE-base to TPCC-BDB-RELEASE... " $cyan -n
cp -r TPCC-BDB-RELEASE-base TPCC-BDB-RELEASE
cecho "done." $ltgreen

##

cecho "Replacing txn_util.c with the modified version... " $cyan -n
if [[ ! -f files/txn_util.c ]]; then
	cecho "Couldn't find files/txn_util.c. Exiting." $ltred
	exit
fi
cp files/txn_util.c db-$BERKELEYDB_VERSION/src/txn/
cecho "done." $ltgreen

##

cecho "Applying the patch on db-$BERKELEYDB_VERSION and TPCC-BDB-RELEASE..." $cyan
patch -p1 < $PATCH_NAME | sed "s/^/> /"
if [[ "$?" != "0" ]] ; then
	cecho "The patch couldn't be applied. Exiting." $ltred
	exit
else
	cecho "Patch successfully applied." $ltgreen
fi
patch -p0 < dbreg.patch
patch -p0 < db.patch

cecho "Replacing mutex_int.h with the modified version... " $cyan -n
if [[ ! -f files/mutex_int.h ]]; then
	cecho "Couldn't find files/mutex_int.h. Exiting." $ltred
	exit
fi
cp db-$BERKELEYDB_VERSION/src/dbinc/mutex_int.h files/mutex_int_backup.h
cp files/mutex_int.h db-$BERKELEYDB_VERSION/src/dbinc/
cecho "done." $ltgreen

##

cecho "Replacing db.c with the modified version... " $cyan -n
if [[ ! -f files/db.c ]]; then
	cecho "Couldn't find files/db.c. Exiting." $ltred
	exit
fi
cp db-$BERKELEYDB_VERSION/src/db/db.c files/db_backup.c
cp files/db.c db-$BERKELEYDB_VERSION/src/db/
cecho "done." $ltgreen

##

cecho "Adding liblock-config.c to src/db/... " $cyan -n
if [[ ! -f files/liblock-config.c ]]; then
	cecho "Couldn't find files/liblock-config.c. Exiting." $ltred
	exit
fi
cp files/liblock-config.c db-$BERKELEYDB_VERSION/src/db/
cecho "done." $ltgreen

##

cecho "Adding liblock-config.h to src/db/... " $cyan -n
if [[ ! -f files/liblock-config.h ]]; then
	cecho "Couldn't find files/liblock-config.h. Exiting." $ltred
	exit
fi
cp files/liblock-config.h db-$BERKELEYDB_VERSION/src/db/
cecho "done." $ltgreen


##

cecho "Adding macro functions... " $cyan -n
cat files/macros.c >> db-$BERKELEYDB_VERSION/src/dbinc/mutex.h
cecho "done." $ltgreen

##

configure_make_berkeleydb Makefile-patched

##

cecho "Moving db-$BERKELEYDB_VERSION to db-$BERKELEYDB_VERSION-patched... " $cyan -n
mv db-$BERKELEYDB_VERSION db-$BERKELEYDB_VERSION-patched
cecho "done." $ltgreen

##

cecho "#######################################################################"\
"##################" $ltred
cecho "# TPC - C w/ Berkeley DB patched                                       "\
"                 #" $ltred
cecho "#######################################################################"\
"##################" $ltred

##

cecho "Deleting TPCC-BDB-RELEASE-patched... " $cyan -n
rm -rf TPCC-BDB-RELEASE-patched
cecho "done." $ltgreen

##

cecho "Moving TPCC-BDB-RELEASE to TPCC-BDB-RELEASE-patched... " $cyan -n
mv TPCC-BDB-RELEASE TPCC-BDB-RELEASE-patched
cecho "done." $ltgreen

##

cecho "Compiling TPC-C (output in logs/tpcc-patched-make.log)..." $cyan
cd TPCC-BDB-RELEASE-patched
sed "s/DBDIR=.*/DBDIR=..\/db-5.2.28.NC-patched\/build_unix/" Makefile > Makefile-mod
mv Makefile-mod Makefile
sed "s/CPPFLAGS=/CPPFLAGS=-DBENCHMARK /" Makefile > Makefile-mod
mv Makefile-mod Makefile
make clean > ../logs/tpcc-berkeleydb-patched-make.log
make >> ../logs/tpcc-berkeleydb-patched-make.log
if [[ "$?" != "0" ]] ; then
	cecho "An error has occured: check the log. Exiting." $ltred
	exit
else
	cecho "TPC-C was compiled successfully." $ltgreen
fi
cd ..

##


