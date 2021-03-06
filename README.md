# DiffKemp

Tool for semantic **Diff**erence of **Ke**rnel functions, **m**odules, and
**p**arameters.

## Usage
First, generate snapshots for each kernel version that you need to compare:

    bin/diffkemp generate KERNEL_DIR SNAPSHOT_DIR FUNCTION_LIST

The command creates a DiffKemp snapshot for semantic diff of functions from
`FUNCTION_LIST` for the kernel located in `KERNEL_DIR` and stores it into
`SNAPSHOT_DIR`. 

After that, run the actual semantic comparison:

    bin/diffkemp compare SNAPSHOT_DIR_1 SNAPSHOT_DIR_2 --show-diff

The command compares functions from function lists stored inside the snapshots
pairwise and prints syntactic diffs (thanks to the `--syntax-diff` option) of
functions that are semantically different.

## About
The tool uses static analysis methods to automatically determine how the effect
of a chosen kernel function or option (module parameter, sysctl) changed between
two different kernel versions.

The analysis is composed of multiple steps:
* Generate: 
    * The source files containing definitions of the compared functions are
      compiled into the LLVM internal representation (LLVM IR). 
    * The snapshot is created by copying the compiled LLVM IR files into the
      snapshot directory and by creating a YAML file with the list of functions
      to be compared.
* Compare: 
    * The **SimpLL** component is used to compare the programs for syntax and
      simple semantic equality. The list of functions that are compared as not
      equal are returned.
    * Optionally, external tools can be used for futher comparison of semantics.
      Currently, we support the **LLReve** tool. It can be used to generate a
      first order logic formula expressing the fact that the remaining programs
      are semantically equal which is then solved using an automatic SMT solver
      *Z3* and the result determines whether the programs are semantically equal
      or not.
    * Finally, for all functions and macros that are found to be semantically
      different, result of the standard `diff` command is shown.

## Components
* LLVM source builder: finding and building kernel source files into LLVM IR.
  * Sources with function definitions are found using CScope. 
  * C sources are built into LLVM IR by checking the commands that are run by
    KBuild for building that file and by replacing GCC by Clang in the command.
* SimpLL: Comparison of programs for syntactic and simple semantic equivalence.
  Does the following steps:
  * Simplification of the compared programs. Multiple transformations are
    applied:
      * If comparing kernel options, slicing out the code that is not influenced
        by the value of the given option.
      * Dead code elimination.
      * Removal of code dependent on line numbers, file names, etc.
      * ... and many others.
  * Comparing programs for syntactic and simple semantic equality. Programs are
    compared instruction-by-instruction.
  * Removing bodies of functions that are proved to be equal. 

## Running environment

Currently, DiffKemp runs on Linux and needs the following software installed:
* LLVM 5.0 (currently the only supported version)
* Python 3 with CFFI
* Python packages from `requirements.txt`
* CScope

## Development

For a better developer experience, there is a development container image
prepared that can be retrieved from DockerHub:
[https://hub.docker.com/r/viktormalik/diffkemp-devel/](https://hub.docker.com/r/viktormalik/diffkemp-devel/)

After that, the container can be run using

    docker/diffkemp-devel/run-container.sh

The script mounts the current directory (the root DiffKemp directory) as a
volume inside the container.

Building DiffKemp outside of the container requires, in addition to the running
dependencies stated above, the following dependencies:
* CMake
* Ninja build system
* Tools neccessary for building the kernels that will be compared.

### Build
	mkdir build
	cd build
	cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
    ninja
    cd ..

Additionally if you are not using the development container:

    pip install -r requirements.txt
    pip install -e .

### Tests

The project contains unit and regression testing using pytest that can be run
by:

    pytest tests

The tests require the sources of the following kernel versions to be stored and
configured in `kernel/linux-{version}` directories:
* 3.10 (upstream kernel)
* 4.11 (upstream kernel)
* 3.10.0-514.el7 (CentOS 7.3 kernel)
* 3.10.0-693.el7 (CentOS 7.4 kernel)
* 3.10.0-862.el7 (CentOS 7.5 kernel)
* 3.10.0-957.el7 (CentOS 7.6 kernel)

The required configuration of each kernel can be done by running:

    make prepare
    make modules_prepare
