#!/usr/bin/env python3
# Copyright © 2020 - 2022 Collabora Ltd.
# Authors:
#   Tomeu Vizoso <tomeu.vizoso@collabora.com>
#   David Heidelberg <david.heidelberg@collabora.com>
#   Guilherme Gallo <guilherme.gallo@collabora.com>
#
# SPDX-License-Identifier: MIT
'''Shared functions between the scripts.'''

import logging
import os
import re
import time
from pathlib import Path

GITLAB_URL = "https://gitlab.freedesktop.org"
TOKEN_DIR = Path(os.getenv("XDG_CONFIG_HOME") or Path.home() / ".config")

# Known GitLab token prefixes: https://docs.gitlab.com/ee/security/token_overview.html#token-prefixes
TOKEN_PREFIXES: dict[str, str] = {
    "Personal access token": "glpat-",
    "OAuth Application Secret": "gloas-",
    "Deploy token": "gldt-",
    "Runner authentication token": "glrt-",
    "CI/CD Job token": "glcbt-",
    "Trigger token": "glptt-",
    "Feed token": "glft-",
    "Incoming mail token": "glimt-",
    "GitLab Agent for Kubernetes token": "glagent-",
    "SCIM Tokens": "glsoat-"
}


def pretty_duration(seconds):
    """Pretty print duration"""
    hours, rem = divmod(seconds, 3600)
    minutes, seconds = divmod(rem, 60)
    if hours:
        return f"{hours:0.0f}h{minutes:0.0f}m{seconds:0.0f}s"
    if minutes:
        return f"{minutes:0.0f}m{seconds:0.0f}s"
    return f"{seconds:0.0f}s"


def get_gitlab_pipeline_from_url(gl, pipeline_url):
    assert pipeline_url.startswith(GITLAB_URL)
    url_path = pipeline_url[len(GITLAB_URL) :]
    url_path_components = url_path.split("/")
    project_name = "/".join(url_path_components[1:3])
    assert url_path_components[3] == "-"
    assert url_path_components[4] == "pipelines"
    pipeline_id = int(url_path_components[5])
    cur_project = gl.projects.get(project_name)
    pipe = cur_project.pipelines.get(pipeline_id)
    return pipe, cur_project


def get_gitlab_project(glab, name: str):
    """Finds a specified gitlab project for given user"""
    if "/" in name:
        project_path = name
    else:
        glab.auth()
        username = glab.user.username
        project_path = f"{username}/{name}"
    return glab.projects.get(project_path)


def get_token_from_default_dir() -> str:
    """
    Retrieves the GitLab token from the default directory.

    Returns:
        str: The path to the GitLab token file.

    Raises:
        FileNotFoundError: If the token file is not found.
    """
    token_file = TOKEN_DIR / "gitlab-token"
    try:
        return str(token_file.resolve())
    except FileNotFoundError as ex:
        print(
            f"Could not find {token_file}, please provide a token file as an argument"
        )
        raise ex


def validate_gitlab_token(token: str) -> bool:
    token_suffix = token.split("-")[-1]
    # Basic validation of the token suffix based on:
    # https://gitlab.com/gitlab-org/gitlab/-/blob/master/gems/gitlab-secret_detection/lib/gitleaks.toml
    if not re.match(r"(\w+-)?[0-9a-zA-Z_\-]{20,64}", token_suffix):
        return False

    for token_type, token_prefix in TOKEN_PREFIXES.items():
        if token.startswith(token_prefix):
            logging.info(f"Found probable token type: {token_type}")
            return True

    # If the token type is not recognized, return False
    return False


def get_token_from_arg(token_arg: str | Path | None) -> str | None:
    if not token_arg:
        logging.info("No token provided.")
        return None

    token_path = Path(token_arg)
    if token_path.is_file():
        return read_token_from_file(token_path)

    return handle_direct_token(token_path, token_arg)


def read_token_from_file(token_path: Path) -> str:
    token = token_path.read_text().strip()
    logging.info(f"Token read from file: {token_path}")
    return token


def handle_direct_token(token_path: Path, token_arg: str | Path) -> str | None:
    if token_path == Path(get_token_from_default_dir()):
        logging.warning(
            f"The default token file {token_path} was not found. "
            "Please provide a token file or a token directly via --token arg."
        )
        return None
    logging.info("Token provided directly as an argument.")
    return str(token_arg)


def read_token(token_arg: str | Path | None) -> str | None:
    token = get_token_from_arg(token_arg)
    if token and not validate_gitlab_token(token):
        logging.warning("The provided token is either an old token or does not seem to "
                        "be a valid token.")
        logging.warning("Newer tokens are the ones created from a Gitlab 14.5+ instance.")
        logging.warning("See https://about.gitlab.com/releases/2021/11/22/"
                        "gitlab-14-5-released/"
                        "#new-gitlab-access-token-prefix-and-detection")
    return token


def wait_for_pipeline(projects, sha: str, timeout=None):
    """await until pipeline appears in Gitlab"""
    project_names = [project.path_with_namespace for project in projects]
    print(f"⏲ for the pipeline to appear in {project_names}..", end="")
    start_time = time.time()
    while True:
        for project in projects:
            pipelines = project.pipelines.list(sha=sha)
            if pipelines:
                print("", flush=True)
                return (pipelines[0], project)
        print("", end=".", flush=True)
        if timeout and time.time() - start_time > timeout:
            print(" not found", flush=True)
            return (None, None)
        time.sleep(1)
