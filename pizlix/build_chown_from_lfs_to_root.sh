#!/bin/bash

set -e
set -x

export LFS=/mnt/lfs

chown --from lfs -R root:root $LFS/{usr,yolo,lib,var,etc,bin,sbin,tools}
chown --from lfs -R root:root $LFS/lib64
