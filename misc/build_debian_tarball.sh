#!/usr/bin/env bash

###
# You may need to edit these periodically
###

VERSION=019c

# everything that goes into a debian tar.gz
read -r -d '' STAGED_PATHS <<EOF
CMakeLists.txt
COPYING.txt
LICENSE.txt
README.txt
cmake
debian
fontstash
master
poly2tri
recast
resource
tnl
zap
EOF

###
# Don't edit these
###

BASENAME=bitfighter-$VERSION
# TODO: find using bash magic
ROOT=.

function stage()
{
	# stage all files
	cp -r $ROOT/$1 $BASENAME

	# then trim untracked files
	for file in `hg status --unknown $ROOT/$1 | cut '--delimiter= ' --fields=2`
	do
		rm $BASENAME/$file
	done
}

# create a workspace
mkdir $BASENAME

# stage all of the files
for path in $(echo "$STAGED_PATHS")
do
	stage $path
done

# clean up any exceptions
rm -r $BASENAME/resource/fonts/
rm -r $BASENAME/resource/screenshots/

# create the tarball, which gets output to the root directory
tar -czf $ROOT/$BASENAME.tar.gz $BASENAME/

# cleanup workspace
rm -r $BASENAME
