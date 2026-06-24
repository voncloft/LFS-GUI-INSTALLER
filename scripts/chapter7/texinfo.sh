name=texinfo
echo "step:Installing $name"

autountar "$name"
cd $name*/

./configure --prefix=/usr
make 
make install
