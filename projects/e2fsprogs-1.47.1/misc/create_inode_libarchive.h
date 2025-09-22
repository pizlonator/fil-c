#ifndef _CREATE_INODE_LIBARCHIVE_H
#define _CREATE_INODE_LIBARCHIVE_H

errcode_t __populate_fs_from_tar(ext2_filsys fs, ext2_ino_t root_ino,
				 const char *source_tar, ext2_ino_t root,
				 struct hdlinks_s *hdlinks,
				 struct file_info *target,
				 struct fs_ops_callbacks *fs_callbacks);

#endif /* _CREATE_INODE_LIBARCHIVE_H */
