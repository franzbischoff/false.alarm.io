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
    Builder for Windows x86 / 32bit
"""
from SCons.Script import (Default, DefaultEnvironment, AlwaysBuild)

from platformio.util import get_systype

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
    SIZEPRINTCMD='$SIZETOOL -A -d $SOURCES',

    #size: supported targets: pe-i386 pei-i386 elf32-i386 elf32-little elf32-big plugin srec symbolsrec verilog tekhex binary ihex
    PROGSUFFIX=".exe")

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
        "-fdata-sections"
    ],
    CXXFLAGS=[
        "-fno-exceptions", "-fno-threadsafe-statics", "-fpermissive",
        "-std=gnu++11"
    ],
    LIBS=["m"],
    CPPDEFINES=[("F_CPU", "$BOARD_F_CPU")],
    LINKFLAGS=[
        "-Wl,--gc-sections", "-static", "-static-libgcc", "-static-libstdc++"
    ])

env.Append(ASFLAGS=env.get("CCFLAGS", [])[:])

if get_systype() == "darwin_x86_64":
    env.Replace(_BINPREFIX="i586-mingw32-")
elif get_systype() in ("linux_x86_64", "linux_i686"):
    env.Replace(_BINPREFIX="i686-w64-mingw32-")

#
# Target: Build executable program
#

target_bin = env.BuildProgram()
# AlwaysBuild(target_bin)

target_elf = env.Alias(
    "sizedata", target_bin,
    env.VerboseAction(
        "$OBJCOPY -O elf32-little $SOURCE $BUILD_DIR/program.elf && cp $BUILD_DIR/program.elf $SOURCE",
        "Calculating size $SOURCE"))

# target_size = env.Alias(
#     "size", target_bin,
#     env.VerboseAction(
#         "$OBJCOPY -O elf32-little $SOURCE $BUILD_DIR/program.elf && cp $BUILD_DIR/program.elf $SOURCE",
#         "Calculating size $SOURCE"))

# target_elf = env.Alias("sizedata", target_size,
#                        env.VerboseAction("echo", "Calculating size $SOURCE"))

#
# Target: Print binary size
#

# target_size = env.Alias(
#     "size", target_bin,
#     env.VerboseAction("$SIZEPRINTCMD", "Calculating size $SOURCE"))

# target_size = env.AddPlatformTarget(
#     "sizedata",
#     target_elf,
#     env.VerboseAction("$SIZEPRINTCMD", "Calculating size $SOURCE"),
#     "Program Size",
#     "Calculate program size",
# )

# AlwaysBuild(target_size)

# AlwaysBuild(target_size)

#
# Default targets
#

# Default(target_elf)
