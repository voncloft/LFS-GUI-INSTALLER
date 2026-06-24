name=gettext
echo "step:Installing $name"

autountar "$name"
cd $name*/

./configure --disable-shared
make
cp -v gettext-tools/src/{msgfmt,msgmerge,xgettext} /usr/bin
