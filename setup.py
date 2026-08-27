import sys
from pathlib import Path
from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension, build_ext

# اكتشاف كافة ملفات C++ تلقائياً
cpp_sources = [str(p) for p in Path("src").rglob("*.cpp")]

extra_compile_args = []
extra_link_args = []

if sys.platform == "win32":
    extra_compile_args.extend(["/std:c++latest", "/O2"])
else:
    extra_compile_args.extend([
        "-std=c++23",
        "-O3",
        "-march=native",
        "-ftree-vectorize",
        "-fvisibility=hidden"
    ])

ext_modules = [
    Pybind11Extension(
        "khcomp._native",
        sources=cpp_sources,
        include_dirs=["include"],
        extra_compile_args=extra_compile_args,
        extra_link_args=extra_link_args,
        cxx_std=23,
    ),
]

setup(
    name="khcomp",
    version="0.1.0",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
)