name=scratchpkg
echo "step:Installing $name"
sh autountar scratchpkg
sh autountar "$name"
cd $name*/

./install
