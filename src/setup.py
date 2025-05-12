from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import pybind11
import sys
import os
from glob import glob

build_dir = os.path.abspath(os.path.join(os.getcwd(), "..", "build"))
print(build_dir)

class BuildExt(build_ext):
    def build_extensions(self):
        # Ensure -fPIC is set for safety
        for ext in self.extensions:
            ext.extra_compile_args = ['-fPIC']
        super().build_extensions()

# include_dirs = [pybind11.get_include(), "draco"] + [i for i in glob("draco/**/*", recursive=True) if os.path.isdir(i)]
include_dirs = [pybind11.get_include(), os.path.join(os.getcwd(), "draco")]
print(include_dirs)

ext_modules = [
    Extension(
        "drc_decoder",
        sources=["drc_decoder.cc", "draco/core/options.cc", "draco/compression/decode.cc", "draco/io/ply_encoder.cc"],
        library_dirs=["../build"],
        include_dirs=include_dirs,
        extra_objects=["../build/libdraco.a",],
        # libraries=["draco"],
        language="c++"
    )
]

setup(
    name="drc_decoder",
    ext_modules=ext_modules,
    cmdclass={"build_ext": BuildExt},
)
