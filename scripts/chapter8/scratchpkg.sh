name=scratchpkg
echo "step:Installing $name"

sh autountar "$name"
cd $name*/

./install
