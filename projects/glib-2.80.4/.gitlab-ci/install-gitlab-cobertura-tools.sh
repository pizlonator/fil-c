#!/usr/bin/env bash

set -e

# We need cobertura reports for gitlab-ci coverage_report,
# so this scripts helps us doing that until we switch to gcovr
pip3 install lcov-cobertura==2.0.2

# We need to split the coverage files, see:
#  https://gitlab.com/gitlab-org/gitlab/-/issues/328772#note_840831654
SPLIT_COBERTURA_SHA512="8388ca3928a27f2ef945a7d45f1dec7253c53742a0dd1f6a3b4a07c0926b24d77f8b5c51fc7920cb07320879b7b89b0e0e13d2101117403b8c052c72e28dbcb7"
wget -O /usr/local/bin/cobertura-split-by-package.py \
   https://gitlab.com/gitlab-org/gitlab/uploads/9d31762a33a10158f5d79d46f4102dfb/split-by-package.py
echo "${SPLIT_COBERTURA_SHA512}  /usr/local/bin/cobertura-split-by-package.py" | sha512sum -c
chmod +x /usr/local/bin/cobertura-split-by-package.py
sed -i "s,\(/usr/bin/env python\).*,\13," \
    /usr/local/bin/cobertura-split-by-package.py
