name=texinfo
echo "step:Installing $name"

sh autountar "$name"
cd $name*/

./configure --prefix=/usr
make 
make install
