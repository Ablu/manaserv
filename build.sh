yum -y install epel-release
yum install -y cmake make libxml2-devel physfs-devel gcc-c++ libsigc++20-devel sqlite-devel lua-devel || exit 1
cmake . || exit 1
make -j$(nproc) || exit 1

