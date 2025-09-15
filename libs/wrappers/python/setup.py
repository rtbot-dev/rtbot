#!/usr/bin/env python3
"""
Minimal setup.py for RtBot Python wrapper.

This setup.py is designed to work with pre-built artifacts from Bazel.
For development builds, use: bazel build //libs/wrappers/python:rtbot_wheel
"""

import os
import sys
from setuptools import setup, find_packages

def get_version():
    """Get version from git tags, with dev suffix if ahead of latest tag."""
    import subprocess

    try:
        # Get the latest git tag
        latest_tag = subprocess.check_output(
            ['git', 'describe', '--tags', '--abbrev=0'],
            stderr=subprocess.PIPE
        ).decode().strip()

        # Remove 'v' prefix if present
        if latest_tag.startswith('v'):
            latest_tag = latest_tag[1:]

        # Get current commit description
        git_describe = subprocess.check_output(
            ['git', 'describe', '--tags', '--always'],
            stderr=subprocess.PIPE
        ).decode().strip()

        # If we're exactly on a tag, use that version
        if git_describe == f'v{latest_tag}' or git_describe == latest_tag:
            return latest_tag

        # If we're ahead of the tag, create a dev version
        # Format: v0.3.8-135-gc6ee14a -> 0.3.8.dev135+gc6ee14a
        parts = git_describe.split('-')
        if len(parts) >= 3:
            tag_part = parts[0]
            if tag_part.startswith('v'):
                tag_part = tag_part[1:]
            commits_ahead = parts[1]
            short_sha = parts[2]
            return f"{tag_part}.dev{commits_ahead}+{short_sha}"

        # Fallback: just use the tag
        return latest_tag

    except (subprocess.CalledProcessError, FileNotFoundError, IndexError):
        # Fallback to reading from Bazel build if git fails
        try:
            version_file = "dist/bin/libs/wrappers/python/version.txt"
            if os.path.exists(version_file):
                with open(version_file, 'r') as f:
                    return f.read().strip()
        except:
            pass
        return "0.1.0"

# Check if we're in development mode (rtbotapi.so exists locally)
has_extension = os.path.exists("rtbotapi.so") or os.path.exists("rtbotapi.pyd")

if not has_extension:
    print("Error: rtbot extension not found.")
    print("This package requires pre-built artifacts from Bazel.")
    print("Run: bazel build //libs/wrappers/python:copy")
    print("Then copy the artifacts to this directory before running setup.py")
    sys.exit(1)

setup(
    name='rtbot',
    version=get_version(),
    description='Python bindings for RtBot framework',
    long_description='RtBot is a real-time data processing framework with Python bindings.',
    long_description_content_type='text/plain',
    author='RtBot Developers',
    url='https://github.com/rtbot-dev/rtbot',
    packages=find_packages(),
    package_data={
        'rtbot': ['*.so', '*.pyd', '*.py'],
    },
    install_requires=[
        "pandas>=1.0.0"
    ],
    python_requires='>=3.10',
    classifiers=[
        'Development Status :: 3 - Alpha',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: Apache Software License',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Programming Language :: Python :: 3.12',
        'Programming Language :: Python :: 3.13',
    ],
)