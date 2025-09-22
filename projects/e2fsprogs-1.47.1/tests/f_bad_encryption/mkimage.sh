#!/bin/bash
#
# This is the script that was used to create the image.gz in this directory.
#
# This requires a patched version of debugfs that understands the "fscrypt."
# xattr name prefix, so that the encryption xattrs can be manipulated.

set -e -u
umask 0022

do_debugfs() {
	umount mnt
	debugfs -w "$@" image
	mount image mnt
}

create_encrypted_file() {
	local file=$1
	local ino

	echo foo > "$file"

	# not needed, but makes image more compressible
	ino=$(stat -c %i "$file")
	do_debugfs -R "zap_block -f <$ino> 0"
}

set_encryption_xattr() {
	local file=$1
	local value=$2
	local ino

	ino=$(stat -c %i "$file")
	do_debugfs -R "ea_set <$ino> fscrypt.c $value"
}

rm_encryption_xattr() {
	local file=$1
	local ino

	ino=$(stat -c %i "$file")
	do_debugfs -R "ea_rm <$ino> fscrypt.c"
}

clear_encrypt_flag() {
	local file=$1
	local ino

	ino=$(stat -c %i "$file")
	do_debugfs -R "set_inode_field <$ino> flags 0"
}

clear_encryption() {
	local file=$1
	local ino
	local is_symlink=false

	if [ -L "$file" ]; then
		is_symlink=true
	fi
	ino=$(stat -c %i "$file")

	do_debugfs -R "ea_rm <$ino> fscrypt.c"
	do_debugfs -R "set_inode_field <$ino> flags 0"
	if $is_symlink; then
		do_debugfs -R "set_inode_field <$ino> block[0] 0xAAAAAAAA"
		do_debugfs -R "set_inode_field <$ino> block[1] 0"
		do_debugfs -R "set_inode_field <$ino> size 4"
	fi
}

mkdir -p mnt
umount mnt &> /dev/null || true

dd if=/dev/zero of=image bs=4096 count=128
mke2fs -O encrypt -b 4096 -N 128 image
mount image mnt

# Create an encrypted directory (ino 12)
dir=mnt/edir
mkdir $dir
echo password | e4crypt add_key $dir

# Control cases: valid encrypted regular file, dir, and symlink (ino 13-15)
create_encrypted_file $dir/encrypted_file
mkdir $dir/encrypted_dir
ln -s target $dir/encrypted_symlink

# Control case: file type that is never encrypted (ino 16)
mkfifo $dir/fifo

# Inodes with missing encryption xattr (ino 17-18).
# e2fsck should offer to clear the encrypt flag on these inodes.

create_encrypted_file $dir/missing_xattr_file
rm_encryption_xattr $dir/missing_xattr_file

mkdir $dir/missing_xattr_dir
rm_encryption_xattr $dir/missing_xattr_dir

# Inodes with corrupt encryption xattr (ino 19-22).
# e2fsck should offer to clear these inodes.

create_encrypted_file $dir/corrupt_xattr_1
set_encryption_xattr $dir/corrupt_xattr_1 '\0'

create_encrypted_file $dir/corrupt_xattr_2
set_encryption_xattr $dir/corrupt_xattr_2 \
	'\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0'

create_encrypted_file $dir/corrupt_xattr_3
set_encryption_xattr $dir/corrupt_xattr_3 '\1'

create_encrypted_file $dir/corrupt_xattr_4
set_encryption_xattr $dir/corrupt_xattr_4 '\2'

# Unencrypted inodes in encrypted directory (ino 23-25).
# e2fsck should offer to clear these directory entries.

create_encrypted_file $dir/unencrypted_file
clear_encryption $dir/unencrypted_file

mkdir $dir/unencrypted_dir
clear_encryption $dir/unencrypted_dir

ln -s target $dir/unencrypted_symlink
clear_encryption $dir/unencrypted_symlink

# Inodes with different encryption policy in encrypted directory (ino 26-29).
# e2fsck should offer to clear these directory entries.

xattr='\1\1\4\0AAAAAAAABBBBBBBBBBBBBBBB'

create_encrypted_file $dir/inconsistent_file_1
set_encryption_xattr $dir/inconsistent_file_1 $xattr

mkdir $dir/inconsistent_dir
set_encryption_xattr $dir/inconsistent_dir $xattr

ln -s target $dir/inconsistent_symlink
set_encryption_xattr $dir/inconsistent_symlink $xattr

xattr='\2\1\4\0\0\0\0\0AAAAAAAAAAAAAAAABBBBBBBBBBBBBBBB'
create_encrypted_file $dir/inconsistent_file_2
set_encryption_xattr $dir/inconsistent_file_2 $xattr

# Encrypted file and directory with valid v2 encryption policy (ino 30-31).
# e2fsck shouldn't change these.
dir2=mnt/edir2
mkdir $dir2
echo password | e4crypt add_key $dir2
xattr='\2\1\4\0\0\0\0\0AAAAAAAAAAAAAAAABBBBBBBBBBBBBBBB'
create_encrypted_file $dir2/file
set_encryption_xattr $dir2/file $xattr
set_encryption_xattr $dir2 $xattr

# Encrypted file and directory with unrecognized encryption policy version
# (ino 32-33).  e2fsck shouldn't change these.
dir3=mnt/edir3
mkdir $dir3
echo password | e4crypt add_key $dir3
xattr='\3'
create_encrypted_file $dir3/file
set_encryption_xattr $dir3/file $xattr
set_encryption_xattr $dir3 $xattr

umount mnt
rmdir mnt
gzip -9 -f image
