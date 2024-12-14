import os
import platform
import subprocess
import sys
import tempfile
import shutil
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
from setuptools.command.egg_info import egg_info

RTBOT_REPO = "https://github.com/rtbot-dev/rtbot.git"

def get_version():
    try:
        output = subprocess.check_output(
            ['bash', '-c', "grep '^VERSION ' dist/out/stable-status.txt | cut -d' ' -f2"],
            stderr=subprocess.PIPE
        ).decode().strip()
        return output if output else "0.1.0"
    except:
        return "0.1.0"

class BazelExtension(Extension):
    def __init__(self, name):
        super().__init__(name, sources=[])

class CustomEggInfo(egg_info):
    def run(self):
        os.makedirs('rtbot', exist_ok=True)
        with open(os.path.join('rtbot', '__init__.py'), 'w') as f:
            f.write('from .rtbotapi import *\n')
        super().run()

class BazelBuildExt(build_ext):
    def run(self):
        self._install_bazelisk()
        repo_dir = self._get_repo_dir()
        self._build_rtbot(repo_dir)
    
    def _get_repo_dir(self):
        current_dir = os.path.abspath(os.getcwd())
        while current_dir != '/':
            if os.path.exists(os.path.join(current_dir, 'WORKSPACE')):
                return current_dir
            current_dir = os.path.dirname(current_dir)
            
        tmp_dir = tempfile.mkdtemp()
        self._clone_repo(tmp_dir)
        return tmp_dir

    def _install_bazelisk(self):
        try:
            subprocess.check_call(['bazelisk', '--version'])
            return
        except (subprocess.CalledProcessError, FileNotFoundError):
            pass

        print("Installing bazelisk...")
        system = platform.system().lower()
        arch = platform.machine().lower()
        
        if system == 'windows':
            url = f"https://github.com/bazelbuild/bazelisk/releases/latest/download/bazelisk-windows-{arch}.exe"
            out = "bazelisk.exe"
        else:
            url = f"https://github.com/bazelbuild/bazelisk/releases/latest/download/bazelisk-{system}-{arch}"
            out = "bazelisk"
        
        subprocess.check_call(['curl', '-L', url, '-o', out])
        if system != 'windows':
            os.chmod(out, 0o755)
        
        os.environ['PATH'] = os.getcwd() + os.pathsep + os.environ['PATH']

    def _clone_repo(self, tmp_dir):
        print("Cloning RTBot repository...")
        subprocess.check_call(['git', 'clone', '--depth', '1', RTBOT_REPO, tmp_dir])

    def _get_bazel_bin(self, repo_dir):
        bazel_cmd = 'bazelisk' if platform.system().lower() != 'windows' else 'bazelisk.exe'
        try:
            bazel_bin = subprocess.check_output(
                [bazel_cmd, 'info', 'bazel-bin'],
                cwd=repo_dir,
                text=True
            ).strip()
            return bazel_bin
        except subprocess.CalledProcessError:
            return os.path.join(repo_dir, 'dist')  # Fallback to --symlink_prefix value

    def _build_rtbot(self, repo_dir):
        print("Building RTBot...")
        bazel_cmd = 'bazelisk' if platform.system().lower() != 'windows' else 'bazelisk.exe'
        
        subprocess.check_call(
            [bazel_cmd, 'build', '//libs/wrappers/python:rtbotapi.so', '//libs/wrappers/python:copy'],
            cwd=repo_dir
        )
        
        bazel_bin = self._get_bazel_bin(repo_dir)
        package_dir = os.path.join(self.build_lib, 'rtbot')
        os.makedirs(package_dir, exist_ok=True)

        # Copy files from bazel-bin
        copy_dir = os.path.join(bazel_bin, 'libs/wrappers/python/rtbot')
        for item in ['MANIFEST.in', 'README.md', 'operators.py', 'setup.py']:
            src = os.path.join(copy_dir, item)
            if os.path.exists(src):
                shutil.copy2(src, package_dir)

        # Copy the extension
        ext_path = os.path.join(bazel_bin, 'libs/wrappers/python/rtbotapi.so')
        if platform.system().lower() == 'windows':
            ext_path = ext_path.replace('.so', '.pyd')
        
        if os.path.exists(ext_path):
            shutil.copy2(ext_path, package_dir)
        else:
            raise RuntimeError(f"Built extension not found at {ext_path}")

setup(
    name='rtbot',
    version=get_version(),
    description='Python bindings for RTBot framework',
    author='RTBot Developers',
    url='https://github.com/rtbot-dev/rtbot',
    ext_modules=[BazelExtension('rtbot')],
    cmdclass={
        'build_ext': BazelBuildExt,
        'egg_info': CustomEggInfo,
    },
    packages=['rtbot'],
    python_requires='>=3.10',
)