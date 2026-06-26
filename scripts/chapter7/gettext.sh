name=gettext
echo "step:Installing $name"

sh autountar "$name"
cd $name*/

./configure --disable-shared
make
cp -v gettext-tools/src/{msgfmt,msgmerge,xgettext} /usr/bin
