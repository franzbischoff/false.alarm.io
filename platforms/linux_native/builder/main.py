# Copyright 2014-present PlatformIO <contact@platformio.org>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""
    Builder for Linux Linux x64_64 / 64-bit
"""
import json
import os
import click
from SCons.Script import (  # pylint: disable=reportMissingImports
    AlwaysBuild,
    Default,
    DefaultEnvironment,
    Import,
)  # , Builder

from platformio.util import get_systype

# A full list with the available variables
# http://www.scons.org/doc/production/HTML/scons-user.html#app-variables
env = DefaultEnvironment()

env.Replace(
    _BINPREFIX="",
    AR="${_BINPREFIX}ar",
    AS="${_BINPREFIX}as",
    CC="${_BINPREFIX}gcc",
    CXX="${_BINPREFIX}g++",
    GDB="${_BINPREFIX}gdb",
    OBJCOPY="${_BINPREFIX}objcopy",
    RANLIB="${_BINPREFIX}ranlib",
    SIZETOOL="${_BINPREFIX}size",
    SIZEPROGREGEXP=r"^(?:\.text|\.data|\.bootloader)\s+(\d+).*",
    SIZEDATAREGEXP=r"^(?:\.data|\.bss|\.noinit)\s+(\d+).*",
    SIZECHECKCMD="$SIZETOOL -A -d $SOURCES",
    SIZEPRINTCMD="$SIZETOOL -A -d $SOURCES",
    # SIZEPRINTCMD="$SIZETOOL $SOURCES",
)

env.Append(
    # LINKFLAGS=[
    # "-static",
    # "-static-libgcc",
    # "-static-libstdc++"
    # ]
    ASFLAGS=["-x", "assembler-with-cpp"],
    CFLAGS=["-std=gnu11", "-fno-fat-lto-objects"],
    CCFLAGS=[
        "-Os",  # optimize for size
        "-Wall",  # show warnings
        "-ffunction-sections",  # place each function in its own section
        "-fdata-sections",
    ],
    CXXFLAGS=[
        "-fno-exceptions",
        "-fno-threadsafe-statics",
        "-fpermissive",
        "-std=gnu++11",
    ],
    LIBS=["m"],
    CPPDEFINES=[("F_CPU", "$BOARD_F_CPU")],
    LINKFLAGS=["-Wl,--gc-sections", "-static", "-static-libgcc", "-static-libstdc++"],
)

env.Append(ASFLAGS=env.get("CCFLAGS", [])[:])

#
# Default flags for bare-metal programming (without any framework layers)
#
# if not env.get("PIOFRAMEWORK"):
#     env.SConscript("frameworks/_bare.py", exports="env")
# env.Append(
#     # ARFLAGS=["..."],
#     #        ASFLAGS=["flag1", "flag2", "flagN"],
#     #        CCFLAGS=["flag1", "flag2", "flagN"],
#     #        CXXFLAGS=["flag1", "flag2", "flagN"],
#     #        LINKFLAGS=["flag1", "flag2", "flagN"],
#     #        CPPDEFINES=["DEFINE_1", "DEFINE=2", "DEFINE_N"],
#     #        LIBS=["additional", "libs", "here"],
#     BUILDERS=dict(ElfToBin=Builder(action=env.VerboseAction(
#         " ".join(["$OBJCOPY", "-O", "binary", "$SOURCES", "$TARGET"]),
#         "Building $TARGET"),
#                                    suffix=".bin")))

if get_systype() == "darwin_x86_64":
    env.Replace(_BINPREFIX="x86_64-pc-linux-")

# The source code of "platformio-build-tool" is here
# https://github.com/platformio/platformio-core/blob/develop/platformio/builder/tools/platformio.py

#
# Target: Build executable program [and linkable fw?]
#

target_bin = env.BuildProgram()

target_elf = env.Alias(
    "sizedata",
    target_bin,
    env.VerboseAction(
        "$OBJCOPY -O elf32-little $SOURCE $BUILD_DIR/program.elf && cp $BUILD_DIR/program.elf $SOURCE",
        "Calculating size $SOURCE",
    ),
)

#
# Target: Build the .bin file
#
# target_bin = env.ElfToBin(join("$BUILD_DIR", "firmware"), target_elf)

#
# Target: Print binary size
#

# env.Depends(target_size, "checkprogsize")

# target_size = env.AddTarget(
#     "sizedata",
#     target_bin,
#     env.VerboseAction("$SIZEPRINTCMD", "Calculating size $SOURCE"),
#     "Program Size",
#     "Calculate program size",
# )

# AlwaysBuild(target_size)


#
# Target: Upload firmware
#
# upload = env.Alias(["upload"], target_bin, "$UPLOADCMD")
# AlwaysBuild(upload)

#
# Default targets
#

# Default([target_bin])
