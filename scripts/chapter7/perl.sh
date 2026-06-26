source ../universal/versions.sh

name=perl
echo "step:Installing $name"

sh autountar "$name"
cd $name*/

sh Configure -des                                         \
             -D prefix=/usr                               \
             -D vendorprefix=/usr                         \
             -D useshrplib                                \
             -D privlib=/usr/lib/perl5/${perl_version%.*}/core_perl     \
             -D archlib=/usr/lib/perl5/${perl_version%.*}/core_perl     \
             -D sitelib=/usr/lib/perl5/${perl_version%.*}/site_perl     \
             -D sitearch=/usr/lib/perl5/${perl_version%.*}/site_perl    \
             -D vendorlib=/usr/lib/perl5/${perl_version%.*}/vendor_perl \
             -D vendorarch=/usr/lib/perl5/${perl_version%.*}/vendor_perl

make
make install
