import sys
import os
from pathlib import Path
from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension, build_ext

# اكتشاف ملفات C++ تلقائياً
cpp_sources = sorted([str(p) for p in Path("src").rglob("*.cpp")])

def get_compile_args():
    """أعلام التجميع الخاصة بكل مجمع ونظام تشغيل"""
    args = ["-O3"]
    
    if sys.platform == "win32":
        # MSVC flags
        return ["/std:c++latest", "/O2", "/permissive-", "/W4"]
    
    # GCC/Clang flags
    args.extend(["-std=c++23", "-fvisibility=hidden", "-ftree-vectorize", "-fPIC"])
    args.extend(["-Wall", "-Wextra", "-Wpedantic"])
    
    # تفعيل march=native محلياً فقط وتجاهله داخل الـ CI لمنع كسر التجميع
    if os.environ.get("CROSS_COMPILING") != "1":
        args.append("-march=native")
    
    return args

def get_link_args():
    """أعلام الـ Linker لنظام macOS"""
    if sys.platform == "darwin":
        if os.environ.get("ARCHFLAGS"):
            return []
        return ["-mmacosx-version-min=10.9"]
    return []

ext_modules = [
    Pybind11Extension(
        "khcomp._native",
        sources=cpp_sources,
        include_dirs=["include"],
        extra_compile_args=get_compile_args(),
        extra_link_args=get_link_args(),
        cxx_std=23,
        language="c++",
    ),
]

setup(
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
)