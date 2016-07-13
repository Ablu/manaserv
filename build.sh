sudo yum -y install epel-release
sudo yum install -y cmake make libxml2-devel physfs-devel gcc-c++ libsigc++20-devel sqlite-devel lua-devel || exit 1
cmake -DCMAKE_INSTALL_PREFIX=/usr/ . || exit 1
make -j$(nproc) || exit 1
sudo make install
