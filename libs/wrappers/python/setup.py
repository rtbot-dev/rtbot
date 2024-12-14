import os
import platform
import subprocess
import sys
import tempfile
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
from setuptools.command.egg_info import egg_info

RTBOT_REPO = "https://github.com/rtbot-dev/rtbot.git"

def get_version():
    try:
        output = subprocess.check_output(
            ['bash', '-c', "grep '^VERSION ' bazel-out/stable-status.txt | cut -d' ' -f2"],
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
        # Create the package directory before running egg_info
        os.makedirs('rtbot', exist_ok=True)
        with open(os.path.join('rtbot', '__init__.py'), 'a'):
            pass
        super().run()

class BazelBuildExt(build_ext):
    def run(self):
        self._install_bazelisk()
        with tempfile.TemporaryDirectory() as tmp_dir:
            self._clone_repo(tmp_dir)
            self._build_rtbot(tmp_dir)
    
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

    def _build_rtbot(self, repo_dir):
        print("Building RTBot...")
        bazel_cmd = 'bazelisk' if platform.system().lower() != 'windows' else 'bazelisk.exe'
        
        subprocess.check_call(
            [bazel_cmd, 'build', '//libs/wrappers/python:rtbot_wheel'],
            cwd=repo_dir
        )
        
        wheel_dir = os.path.join(self.build_lib, 'rtbot')
        os.makedirs(wheel_dir, exist_ok=True)
        
        bazel_bin = os.path.join(repo_dir, 'bazel-bin/libs/wrappers/python/rtbot')
        for root, _, files in os.walk(bazel_bin):
            for file in files:
                src = os.path.join(root, file)
                dst = os.path.join(wheel_dir, os.path.relpath(src, bazel_bin))
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                self.copy_file(src, dst)

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
    install_requires=[
        'numpy>=1.19.0',
    ]
)