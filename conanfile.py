from conan import ConanFile
from conan.tools.files import copy
from conan.tools.layout import basic_layout
import os


class EmhashConan(ConanFile):
    name = "emhash"
    version = "1.1.0"
    description = "High-performance, memory-efficient C++ open addressing flat hash table"
    license = "MIT"
    url = "https://github.com/ktprime/emhash"
    homepage = "https://github.com/ktprime/emhash"
    topics = ("hash-map", "hash-table", "header-only", "open-addressing")
    settings = "os", "compiler", "build_type", "arch"
    no_copy_source = True

    def layout(self):
        basic_layout(self)

    def package_id(self):
        self.info.clear()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        headers = [
            "hash_table5.hpp", "hash_table6.hpp", "hash_table7.hpp", "hash_table8.hpp",
            "hash_set2.hpp", "hash_set3.hpp", "hash_set4.hpp", "hash_set8.hpp", "hash_set81.hpp",
            "hash_table7s.hpp", "utils.hpp",
        ]
        for h in headers:
            copy(self, h, src=self.source_folder, dst=os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = ["include"]
