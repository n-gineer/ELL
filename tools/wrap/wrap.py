#!/usr/bin/env python3
####################################################################################################
#
#  Project:  Embedded Learning Library (ELL)
#  File:     wrap.py
#  Authors:  Chris Lovett, Kern Handa
#
#  Requires: Python 3.x
#
####################################################################################################

import argparse
import json
import logging
import os
import platform
import sys
import time
from shutil import copyfile

__script_path = os.path.dirname(os.path.abspath(__file__))
sys.path += [os.path.join(__script_path, "..", "utilities", "pythonlibs")]

import find_ell
import buildtools
import logger

# This script creates a compilable Python project for executing a given ELL model on a target platform.
# Compilation of the resulting project will require a C++ compiler.


class _PassArgsParser(argparse.ArgumentParser):
    def __init__(self, *args, **kwargs):
        super(_PassArgsParser, self).__init__(*args, **kwargs)

    def format_usage(self):
        usage = super(_PassArgsParser, self).format_usage().strip('\n')
        usage += " [-- <compile args>]\n"
        return usage

    def format_help(self):
        usage = super(_PassArgsParser, self).format_usage()
        help = super(_PassArgsParser, self).format_help()
        dashdash_help = "  --                    everything after '--' is passed to the compiler\n"
        return help.replace(usage, self.format_usage()) + dashdash_help


class ModuleBuilder:
    arguments = {
        "model_file":
        {
            "short": "f",
            "required": True,
            "help": "path to the ELL model file"
        },
        "module_name":
        {
            "short": "n",
            "default": None,
            "help": "the name of the output module (defaults to the model filename)"
        },
        "target":
        {
            "short": "t",
            "default": "host",
            "help": "the target platform",
            "choices": ["pi3", "pi0", "orangepi0", "pi3_64", "aarch64", "host"]
        },
        "skip_ellcode":
        {
            "short": "skip_ellcode",
            "default": False,
            "help": "skip ELLCode"
        },
        "language":
        {
            "short": "l",
            "default": "python",
            "help": "the language for the ELL module",
            "choices": ["python", "cpp"]
        },
        "llvm_format":
        {
            "short": "if",
            "default": "bc",
            "help": "the format of the emitted code (default 'bc')",
            "choices": ["ir", "bc", "asm", "obj"]
        },
        "outdir":
        {
            "short": "od",
            "default": None,
            "help": "the output directory"
        },
        "verbose":
        {
            "short": "v",
            "default": False,
            "help": "print verbose output"
        },
        "profile":
        {
            "short": "p",
            "default": False,
            "help": "enable profiling functions in the ELL module"
        },
        "blas":
        {
            "short": "b",
            "default": "true",
            "help": "enable or disable the use of Blas on the target device"
        },
        "no_fuse_linear_ops":
        {
            "short": "no_fuse_linear_ops",
            "default": False,
            "help": "disable the fusing of sequences of linear operations"
        },
        "no_optimize_reorder":
        {
            "short": "no_optimize_reorder",
            "default": False,
            "help": "disable the optimization of reorder data nodes"
        },
        "no_opt_tool":
        {
            "short": "no_opt_tool",
            "default": False,
            "help": "disable the use of LLVM's opt tool"
        },
        "no_llc_tool":
        {
            "short": "no_llc_tool",
            "default": False,
            "help": "disable the use of LLVM's llc tool"
        },
        "no_optimize":
        {
            "short": "no_opt",
            "default": False,
            "help": "disable ELL's compiler from optimizing emitted code"
        },
        "no_parallelize":
        {
            "short": "no_par",
            "default": False,
            "help": "disable ELL's compiler from parallelizing emitted code. \
Only necessary if optimization is enabled"
        },
        "no_vectorize":
        {
            "short": "no_vec",
            "default": False,
            "help": "disable ELL's compiler from generating specially vectorized code. \
Only necessary if optimization is enabled"
        },
        "optimization_level":
        {
            "short": "ol",
            "default": "3",
            "help": "the optimization level used by LLVM's opt and llc tools. \
If '0' or 'g', opt is not run (default '3')",
            "choices": ["0", "1", "2", "3", "g"]
        },
        "debug":
        {
            "short": "dbg",
            "default": False,
            "help": "emit debug code"
        },
        "global_value_alignment":
        {
            "short": "gva",
            "default": 32,
            "help": "The number of bytes to align global buffers to",
            "type": int
        },
        "stats":
        {
            "short": "stats",
            "default": False,
            "help": "write compiler performance stats to 'wrap_stats.json' file"
        }
    }

    def __init__(self):
        self.config = None
        self.files = []
        self.includes = []
        self.tools = None
        self.language = "python"
        self.target = "host"
        self.verbose = False
        self.llvm_format = None
        self.no_opt_tool = False
        self.no_llc_tool = False
        self.profile = False
        self.blas = True
        self.optimize = True
        self.parallelize = True
        self.vectorize = True
        self.optimization_level = None
        self.fuse_linear_ops = True
        self.optimize_reorder = True
        self.debug = False
        self.model_name = ""
        self.func_name = "Predict"
        self.objext = "o"
        self.logger = None
        self.skip_ellcode = False

    def str2bool(self, v):
        return v.lower() in ("yes", "true", "t", "1")

    def get_objext(self, target):
        return "o"

    def parse_command_line(self, args=None):
        arg_parser = _PassArgsParser(prog="wrap", description="""This tool wraps a given ELL model in a CMake buildable \
project that builds a language specific module that can call the ELL model on a given target platform.
The supported languages are:
    python   (default)
    cpp
The supported target platforms are:
    pi0       Raspberry Pi 0
    pi3       Raspberry Pi 3
    orangepi0 Orange Pi Zero
    aarch64   arm64 Linux, works on Qualcomm DragonBoards
    host      (default) your host computer architecture""")

        for arg in self.arguments.keys():
            argdef = self.arguments[arg]
            arg_type = str
            if "type" in argdef.keys():
                arg_type = argdef["type"]
            if "required" in argdef.keys():
                arg_parser.add_argument("--" + arg, "-" + argdef["short"],
                                        help=argdef["help"], type=arg_type, required=True)
            elif "choices" in argdef.keys():
                arg_parser.add_argument("--" + arg, "-" + argdef["short"],
                                        help=argdef["help"], type=arg_type, default=argdef["default"],
                                        choices=argdef["choices"])
            elif type(argdef["default"]) is bool and not argdef["default"]:
                arg_parser.add_argument("--" + arg, "-" + argdef["short"],
                                        help=argdef["help"], action="store_true", default=False)
            else:
                arg_parser.add_argument("--" + arg, "-" + argdef["short"],
                                        help=argdef["help"], type=arg_type, default=argdef["default"])

        compile_args = []
        if '--' in args:
            index = args.index('--')
            compile_args = args[index + 1:]
            args = args[:index]

        logger.add_logging_args(arg_parser)
        args = arg_parser.parse_args(args)
        self.logger = logger.setup(args)

        self.model_file = args.model_file
        _, tail = os.path.split(self.model_file)
        self.model_file_base = os.path.splitext(tail)[0]
        self.model_name = args.module_name
        if not self.model_name:
            self.model_name = self.model_file_base.replace('-', '_')
        self.language = args.language
        self.target = args.target
        self.skip_ellcode = args.skip_ellcode
        self.objext = self.get_objext(self.target)
        self.output_dir = args.outdir
        if self.output_dir is None:
            self.output_dir = self.target
        if os.path.isfile(self.output_dir + ".py"):
            raise Exception("You have a python module named '{}', which will conflict with the --outdir of '{}'. \
Please specify a different outdir.".format(self.output_dir + ".py", self.output_dir))
        self.profile = args.profile

        self.verbose = self.logger.getVerbose() or args.verbose
        if args.verbose:
            self.logger.logger.setLevel(logging.INFO)
        self.llvm_format = args.llvm_format
        self.optimization_level = args.optimization_level
        self.no_opt_tool = args.no_opt_tool or self.optimization_level in ['0', 'g']
        self.no_llc_tool = args.no_llc_tool
        self.optimize = not args.no_optimize
        self.parallelize = self.optimize and not args.no_parallelize
        self.vectorize = self.optimize and not args.no_vectorize
        self.fuse_linear_ops = not args.no_fuse_linear_ops
        self.optimize_reorder = not args.no_optimize_reorder
        self.debug = args.debug
        self.blas = self.str2bool(args.blas)
        self.swig = self.language != "cpp"
        self.cpp_header = self.language == "cpp"
        self.compile_args = compile_args
        self.global_value_alignment = args.global_value_alignment
        self.stats = args.stats
        self.times = {}

    def find_files(self):
        __script_path = os.path.dirname(os.path.abspath(__file__))
        self.cmake_template = os.path.join(__script_path, "templates/CMakeLists.%s.txt.in" % (self.language))

        if not os.path.isfile(self.cmake_template):
            raise Exception("Could not find CMakeLists template: %s" % (self.cmake_template))

        if self.language == "python":
            self.module_init_template = os.path.join(__script_path, "templates/__init__.py.in")

            if not os.path.isfile(self.module_init_template):
                raise Exception("Could not find __init__.py template: %s" % (self.module_init_template))

        self.files.append(os.path.join(self.ell_root, "CMake/OpenBLASSetup.cmake"))

    def copy_files(self, filelist, folder):
        if not folder:
            target_dir = self.output_dir
        else:
            target_dir = os.path.join(self.output_dir, folder)

        os.makedirs(target_dir, exist_ok=True)

        for path in filelist:
            if not os.path.isfile(path):
                raise Exception("expected file not found: " + path)

            _, file_name = os.path.split(path)
            dest = os.path.join(target_dir, file_name)

            if self.verbose:
                self.logger.info("copy \"%s\" \"%s\"" % (path, dest))

            copyfile(path, dest)

    def create_template_file(self, template_filename, output_filename):
        with open(template_filename) as f:
            template = f.read()

        template = template.replace("@ELL_outdir@", os.path.basename(self.output_dir))
        template = template.replace("@ELL_model@", self.model_file_base)
        template = template.replace("@ELL_model_name@", self.model_name)
        template = template.replace("@Arch@", self.target)
        template = template.replace("@OBJECT_EXTENSION@", self.objext)
        template = template.replace("@ELL_ROOT@", os.path.join(self.ell_root, "external").replace("\\", "/"))
        shell_type = "UNIX"
        if self.target == "host" and platform.system() == "Windows":
            shell_type = "WINDOWS"
        template = template.replace("@SHELL_TYPE@", shell_type)
        output_template = os.path.join(self.output_dir, output_filename)
        with open(output_template, 'w') as f:
            f.write(template)

    def create_cmake_file(self):
        self.create_template_file(self.cmake_template, "CMakeLists.txt")

    def create_module_init_file(self):
        self.create_template_file(self.module_init_template, "__init__.py")

    def save_config(self):
        self.config['model'] = self.model_name
        self.config['func'] = self.model_name + "_" + self.func_name
        config_json = json.dumps(self.config, indent=2, sort_keys=True)
        _, tail = os.path.split(self.config_file)
        outputFile = os.path.join(self.output_dir, tail)
        self.logger.info("creating config file: '" + outputFile + "'")
        with open(outputFile, 'w') as f:
            f.write(config_json)
            f.close()

    def start_timer(self, name):
        self.times[name] = time.time()

    def stop_timer(self, name):
        self.times[name] = time.time() - self.times[name]

    def write_stats(self):
        if self.stats:
            filename = os.path.join(self.output_dir, "wrap_stats.json")
            with open(filename, 'w') as f:
                json.dump(self.times, f, indent=2)

    def run(self):
        self.build_root = find_ell.find_ell_build()
        self.ell_root = os.path.dirname(self.build_root)
        self.tools = buildtools.EllBuildTools(self.ell_root)
        self.find_files()
        self.copy_files(self.files, "")
        self.copy_files(self.includes, "include")
        self.start_timer("compile")
        out_file = self.tools.compile(
            model_file=self.model_file,
            func_name=self.func_name,
            model_name=self.model_name,
            target=self.target,
            skip_ellcode=self.skip_ellcode,
            output_dir=self.output_dir,
            use_blas=self.blas,
            fuse_linear_ops=self.fuse_linear_ops,
            optimize_reorder_data_nodes=self.optimize_reorder,
            profile=self.profile,
            llvm_format=self.llvm_format,
            optimize=self.optimize,
            parallelize=self.parallelize,
            vectorize=self.vectorize,
            debug=self.debug,
            is_model_file=False,
            swig=self.swig,
            header=self.cpp_header,
            objext="." + self.objext,
            global_value_alignment=self.global_value_alignment,
            extra_options=self.compile_args)
        self.stop_timer("compile")
        if self.swig:
            self.start_timer("swig")
            args = []
            if self.target in ["pi3", "pi0", "orangepi0"]:
                args += ["-DSWIGWORDSIZE32", "-DLINUX"]
            elif self.target in ["pi3_64", "aarch64"]:
                args += ["-DSWIGWORDSIZE64", "-DLINUX"]
            elif self.target == "host":
                from sys import maxsize
                if sys.platform.startswith('win32'):
                    args += ["-DWIN32"]
                    # SWIG expects 32-bit, regardless of bitness for Windows
                    # (because INT_MAX == LONG_MAX on Windows)
                    args += ["-DSWIGWORDSIZE32"]
                else:
                    if sys.platform.startswith('linux'):
                        args += ["-DLINUX"]
                        if maxsize > 2**32:
                            args += ["-DSWIGWORDSIZE64"]
                        else:
                            args += ["-DSWIGWORDSIZE32"]
                    elif sys.platform.startswith('darwin'):
                        args += ["-DAPPLE"]
                    else:
                        pass  # raise exception?
            else:
                pass  # raise exception?
            self.tools.swig(self.output_dir, self.model_file_base, self.language, args)
            self.stop_timer("swig")
        if not self.no_opt_tool:
            self.start_timer("opt")
            out_file = self.tools.opt(self.output_dir, out_file, self.optimization_level)
            self.stop_timer("opt")
        if not self.no_llc_tool:
            self.start_timer("llc")
            out_file = self.tools.llc(self.output_dir, out_file, self.target, self.optimization_level,
                                      "." + self.objext)
            self.stop_timer("llc")
        self.write_stats()
        self.create_cmake_file()
        if self.language == "python":
            self.create_module_init_file()
        if self.target == "host":
            self.logger.info("success, now you can build the '" + self.output_dir + "' folder")
        else:
            self.logger.info("success, now copy the '{}' folder to your target machine and build it there".format(
                self.output_dir))


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    builder = ModuleBuilder()
    builder.parse_command_line(sys.argv[1:])
    try:
        builder.run()
    except:
        errorType, value, traceback = sys.exc_info()
        msg = "### WrapException: %s: %s" % (str(errorType), str(value))
        logger.get().error(msg)
        sys.exit(1)
