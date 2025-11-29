## GTK CI infrastructure

GTK uses different CI images depending on platform and jobs.

The CI images are Docker containers, generated either using `docker` or
`podman`, and pushed to the GitLab [container registry][registry].

Each Docker image has a tag composed of two parts:

 - `${image}`: the base image for a given platform, like "fedora" or
   "debian-stable"
 - `${number}`: an incremental version number, or `latest`

See the [container registry][registry] for the available images for each
branch, as well as their available versions.

Note that using `latest` as version number will overwrite the most
recently uploaded image in the registry.

### Checklist for Updating a CI image

 - [ ] Update the `${image}.Dockerfile` file with the dependencies
 - [ ] Run `./run-docker.sh build --base ${image} --version ${number}`
 - [ ] Run `./run-docker.sh push --base ${image} --version ${number}`
   once the Docker image is built; you may need to log in by using
   `docker login` or `podman login`
 - [ ] Update the `image` keys in the `.gitlab-ci.yml` file with the new
   image tag
 - [ ] Open a merge request with your changes and let it run

### Checklist for Adding a new CI image

 - [ ] Write a new `${image}.Dockerfile` with the instructions to set up
   a build environment
 - [ ] Add the `pip3 install meson` incantation
 - [ ] Run `./run-docker.sh build --base ${image} --version ${number}`
 - [ ] Run `./run-docker.sh push --base ${image} --version ${number}`
 - [ ] Add the new job to `.gitlab-ci.yml` referencing the image
 - [ ] Open a merge request with your changes and let it run

### Checklist for Adding a new dependency to a CI image

Our images are layered, and the base (called fedora-base) contains
all the rpm payload. Therefore, adding a new dependency is a 2-step
process:

 1. [ ] Build and upload fedora-base:$version+1
 1. [ ] Build and upload fedora:$version+1 based on fedora-base:version+1

[registry]: https://gitlab.gnome.org/GNOME/gtk/container_registry
