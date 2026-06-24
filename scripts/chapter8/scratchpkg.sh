name=scratchpkg
echo "step:Installing $name"

autountar "$name"
cd $name*/

./install
