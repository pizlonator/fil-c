#!/usr/bin/env python3
#
# Makes a GNU-Style ChangeLog from a git repository
import os
import sys
import subprocess
import re

meson_source_root = os.environ.get('MESON_SOURCE_ROOT')

meson_dist_root = os.environ.get('MESON_DIST_ROOT')
if meson_dist_root:
    output_fn = os.path.join(meson_dist_root, 'ChangeLog')
else:
    output_fn = sys.stdout.fileno()

# commit hash => release version tag string
release_refs = {}

# These are the pre-monorepo module beginnings
changelog_starts = {
    'gstreamer': '70521179a75db0c7230cc47c6d7f9d63cf73d351',
    'gst-plugins-base': '68746a38d5e48e6f7c220663dcc2f175ff55cb3c',
    'gst-plugins-good': '81f63142d65b62b0971c19ceb79956c49ffc2f06',
    'gst-plugins-ugly': '7d7c3e478e32b7b66c44cc4442d571fbab534740',
    'gst-plugins-bad': 'ea6821e2934fe8d356ea89d5610f0630b3446877',
    'gst-libav': '3c440154c60d1ec0a54186f0fad4aebfd2ecc3ea',
    'gst-rtsp-server': '5029c85a46a8c366c4bf272d503e22bbcd624ece',
    'gst-editing-services': 'ee8bf88ebf131cf7c7161356540efc20bf411e14',
    'gst-python': 'b3e564eff577e2f577d795051bbcca85d47c89dc',
    'gstreamer-vaapi': 'c89e9afc5d43837c498a55f8f13ddf235442b83b',
    'gst-devtools': 'da962d096af9460502843e41b7d25fdece7ff1c2',
    'gstreamer-sharp': 'b94528f8e7979df49fedf137dfa228d8fe475e1b',
}


def print_help():
    print('', file=sys.stderr)
    print('gen-changelog: generate GNU-style changelog from git history',
          file=sys.stderr)
    print('', file=sys.stderr)
    print('Usage: {} [OPTIONS] GSTREAMER-MODULE [START-TAG] [HEAD-TAG]'.format(
        sys.argv[0]), file=sys.stderr)
    print('', file=sys.stderr)
    sys.exit(1)


if len(sys.argv) < 2 or len(sys.argv) > 4 or '--help' in sys.argv:
    print_help()

module = sys.argv[1]

if len(sys.argv) > 2:
    start_tag = sys.argv[2]
else:
    start_tag = None

if len(sys.argv) > 3:
    head_tag = sys.argv[3]
else:
    head_tag = None

if module not in changelog_starts:
    print(f'Unknown module {module}', file=sys.stderr)
    print_help()


def process_commit(lines, files, subtree_path=None):
    # DATE NAME
    # BLANK LINE
    # Subject
    # BLANK LINE
    # ...
    # FILES
    fileincommit = False
    lines = [x.strip() for x in lines if x.strip()
             and not x.startswith('git-svn-id')]
    files = [x.strip() for x in files if x.strip()]
    for line in lines:
        if line.startswith('* ') and ':' in line:
            fileincommit = True
            break

    top_line = lines[0]
    print(top_line.strip())
    print()
    if not fileincommit:
        for f in files:
            if subtree_path and f.startswith(subtree_path):
                # requires Python 3.9
                print('\t* %s:' % f.removeprefix(subtree_path))
            else:
                print('\t* %s:' % f)
    for line in lines[1:]:
        print('\t ', line)
    print()


def output_commits(module, start_tag, end_tag, subtree_path=None):
    # retrieve commit date for start tag so we can filter the log for commits
    # after that date. That way we don't include commits from merged-in
    # plugin-move branches that go back to the beginning of time.
    start_date = get_commit_date_for_ref(start_tag)

    cmd = ['git', 'log',
           '--pretty=format:--START-COMMIT--%H%n%ai  %an <%ae>%n%n%s%n%b%n--END-COMMIT--',
           '--date=short',
           '--name-only',
           f'--since={start_date}',
           f'{start_tag}..{end_tag}',
           ]

    if subtree_path:
        cmd += ['--', '.']

    p = subprocess.Popen(args=cmd, shell=False,
                         stdout=subprocess.PIPE, cwd=meson_source_root)
    buf = []
    files = []
    filemode = False
    for lin in [x.decode('utf8', errors='replace') for x in p.stdout.readlines()]:
        if lin.startswith("--START-COMMIT--"):
            commit_hash = lin[16:].strip()
            if buf != []:
                process_commit(buf, files, subtree_path)

            if commit_hash in release_refs:
                version_str = release_refs[commit_hash]
                print(f'=== release {version_str} ===\n')

            buf = []
            files = []
            filemode = False
        elif lin.startswith("--END-COMMIT--"):
            filemode = True
        elif filemode is True:
            files.append(lin)
        else:
            buf.append(lin)
    if buf != []:
        process_commit(buf, files, subtree_path)


def get_commit_date_for_ref(ref):
    cmd = ['git', 'log', '--pretty=format:%cI', '-1', ref]
    r = subprocess.run(cmd, capture_output=True, text=True,
                       check=True, cwd=meson_source_root)
    commit_date = r.stdout.strip()
    return commit_date


def populate_release_tags_for_premonorepo_module(module_tag_prefix):
    if module_tag_prefix != '':
        cmd = ['git', 'tag', '--list', f'{module_tag_prefix}*']
    else:
        cmd = ['git', 'tag', '--list', '1.*', 'RELEASE-*']

    p = subprocess.Popen(args=cmd, shell=False,
                         stdout=subprocess.PIPE, cwd=meson_source_root)
    for line in [x.decode('utf8') for x in p.stdout.readlines()]:
        git_tag = line.strip()
        version_str = git_tag.removeprefix(module_tag_prefix).removeprefix('RELEASE-').split('-')[0].replace('_', '.')
        # might have been populated with post-monorepo tags already for gstreamer core
        if version_str not in release_refs:
            # find last commit before tag in module subdirectory
            cmd = ['git', 'log', '--pretty=format:%H', '-1', git_tag]
            r = subprocess.run(cmd, capture_output=True,
                               text=True, check=True, cwd=meson_source_root)
            commit_hash = r.stdout.strip()
            release_refs[commit_hash] = version_str

            # print(f'{git_tag} => {version_str} => {commit_hash}')


def populate_release_tags_for_monorepo_subproject():
    cmd = ['git', 'tag', '--list', '1.*']
    p = subprocess.Popen(args=cmd, shell=False,
                         stdout=subprocess.PIPE, cwd=meson_source_root)
    for line in [x.decode('utf8') for x in p.stdout.readlines()]:
        version_str = line.strip()
        version_arr = version_str.split('.')
        major = int(version_arr[0])
        minor = int(version_arr[1])
        micro = int(version_arr[2])
        # ignore pre-monorepo versions
        if major < 1:
            continue
        if major == 1 and minor < 19:
            continue
        if major == 1 and minor == 19 and micro < 2:
            continue
        # find last commit before tag in module subdirectory
        cmd = ['git', 'log', '--pretty=format:%H',
               '-1', version_str, '--', '.']
        r = subprocess.run(cmd, capture_output=True, text=True,
                           check=True, cwd=meson_source_root)
        commit_hash = r.stdout.strip()
        release_refs[commit_hash] = version_str


if __name__ == '__main__':
    module_tag_prefix = '' if module == 'gstreamer' else f'{module}-'

    populate_release_tags_for_monorepo_subproject()

    with open(output_fn, 'w') as f:
        sys.stdout = f

        # Force writing of head tag
        if head_tag and head_tag not in release_refs.values():
            print(f'=== release {head_tag} ===\n')

        # Output all commits from start_tag onwards, otherwise output full history.
        # (We assume the start_tag is after the monorepo merge if it's specified.)
        if start_tag and start_tag != 'start':
            output_commits(module, start_tag, 'HEAD', f'subprojects/{module}/')
        else:
            # First output all post-monorepo commits or commits from start_tag if specified
            output_commits(module, 'monorepo-start',
                           'HEAD', f'subprojects/{module}/')

            populate_release_tags_for_premonorepo_module(module_tag_prefix)

            # Next output all pre-monorepo commits (modules have their own root)
            if not start_tag:
                module_start = f'{module_tag_prefix}1.0.0'
            elif start_tag == 'start':
                module_start = changelog_starts[module]
            else:
                module_start = f'{module_tag_prefix}{start_tag}'

            output_commits(module, module_start,
                           f'{module_tag_prefix}1.19.2', None)

        # Write start tag at end for clarity
        if not start_tag:
            print(f'=== release 1.0.0 ===\n')
        elif start_tag != 'start':
            print(f'=== release {start_tag} ===\n')
