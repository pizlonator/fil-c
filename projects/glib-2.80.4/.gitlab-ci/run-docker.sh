#!/bin/bash

read_arg() {
    # $1 = arg name
    # $2 = arg value
    # $3 = arg parameter
    local rematch='^[^=]*=(.*)$'
    if [[ $2 =~ $rematch ]]; then
        read -r "$1" <<< "${BASH_REMATCH[1]}"
    else
        read -r "$1" <<< "$3"
        # There is no way to shift our callers args, so
        # return 1 to indicate they should do it instead.
        return 1
    fi
}

if type -p podman; then
        # Using podman
        DOCKER_CMD="podman"
        # Docker is actually implemented by podman, and its OCI output
        # is incompatible with some of the dockerd instances on GitLab
        # CI runners.
        export BUILDAH_FORMAT=docker
elif getent group docker | grep -q "\b${USER}\b"; then
        DOCKER_CMD="docker"
else
        DOCKER_CMD="sudo docker"
fi

set -e

base=""
base_version=""
build=0
run=0
push=0
list=0
print_help=0
no_login=0

while (($# > 0)); do
        case "${1%%=*}" in
                build) build=1;;
                run) run=1;;
                push) push=1;;
                list) list=1;;
                help) print_help=1;;
                --base|-b) read_arg base "$@" || shift;;
                --base-version) read_arg base_version "$@" || shift;;
                --no-login) no_login=1;;
                *) echo -e "\\e[1;31mERROR\\e[0m: Unknown option '$1'"; exit 1;;
        esac
        shift
done

if [ $print_help == 1 ]; then
        echo "$0 - Build and run Docker images"
        echo ""
        echo "Usage: $0 <command> [options] [basename]"
        echo ""
        echo "Available commands"
        echo ""
        echo "  build --base=<BASENAME> - Build Docker image <BASENAME>.Dockerfile"
        echo "  run --base=<BASENAME>   - Run Docker image <BASENAME>"
        echo "  push --base=<BASENAME>  - Push Docker image <BASENAME> to the registry"
        echo "  list                    - List available images"
        echo "  help                    - This help message"
        echo ""
        exit 0
fi

cd "$(dirname "$0")"

if [ $list == 1 ]; then
        echo "Available Docker images:"
        for f in *.Dockerfile; do
                filename=$( basename -- "$f" )
                basename="${filename%.*}"

                echo -e "  \\e[1;39m$basename\\e[0m"
        done
        exit 0
fi

# All commands after this require --base to be set
if [ -z "${base}" ]; then
        echo "Usage: $0 <command>"
        exit 1
fi

if [ ! -f "$base.Dockerfile" ]; then
        echo -e "\\e[1;31mERROR\\e[0m: Dockerfile for '$base' not found"
        exit 1
fi

if [ -z "${base_version}" ]; then
        base_version="latest"
else
        base_version="v$base_version"
fi

TAG="registry.gitlab.gnome.org/gnome/glib/${base}:${base_version}"

if [ $build == 1 ]; then
        echo -e "\\e[1;32mBUILDING\\e[0m: ${base} as ${TAG}"
        $DOCKER_CMD build \
                --build-arg HOST_USER_ID="$UID" \
                --build-arg COVERITY_SCAN_PROJECT_NAME="${COVERITY_SCAN_PROJECT_NAME}" \
                --build-arg COVERITY_SCAN_TOKEN="${COVERITY_SCAN_TOKEN}" \
                --tag "${TAG}" \
                --file "${base}.Dockerfile" .
        exit $?
fi

if [ $push == 1 ]; then
        echo -e "\\e[1;32mPUSHING\\e[0m: ${base} as ${TAG}"

        if [ $no_login == 0 ]; then
                $DOCKER_CMD login registry.gitlab.gnome.org
        fi

        $DOCKER_CMD push $TAG
        exit $?
fi

if [ $run == 1 ]; then
        echo -e "\\e[1;32mRUNNING\\e[0m: ${base} as ${TAG}"
        $DOCKER_CMD run \
                --rm \
                --volume "$(pwd)/..:/home/user/app" \
                --workdir "/home/user/app" \
                --tty \
                --interactive "${TAG}" \
                bash
        exit $?
fi
