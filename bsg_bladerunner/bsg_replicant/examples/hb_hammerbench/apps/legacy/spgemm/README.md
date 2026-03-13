# Sparse GEMM

Sparse Matrix Multiplication on HammerBlade.

## Setup

Type from the spgemm directory `make setup`

## Running

The test harnesses are setup to be parameterized for inputs and work partitioning.

By default you can run spgemm on the following inputs:

1. offshore
2. wiki-vote
3. road-central
4. roadNet-CA

To run on all of these inputs do the following:

```
cd spmm_abrev_multi_pod_model
make profile -j <threads>
```
### Changing the Inputs

To change the inputs:
1. open `spmm_abrev_multi_pod_model/Makefile` with your favorite editor
2. search for `CHANGE ME: INPUTS`
3. copy a line from one of the inputs already there and replace with your new input
4. remove lines with inputs you don't want to run.

### Changing the Number of Submatrices the Problem is Divided Into

Some reasons why you would want to do this:
* You want to model multiple pods working on the problem cooperatively.
* You can simulate the problem in parts, and these simulations can run in parallel.
* You want to run on a input that is too big to fit in a single pod's memory.

To change the partitioning (i.e. the number of output submatrices the problem is divided into):
1. open `spmm_abrev_multi_pod_model/Makefile` with your favorite editor
2. search for `CHANGE ME: PARTITIONING`
3. change `PFACTOR` to `K` to divide the output into `K^2` submatrices


### Abreviated Simulation

You can run spgemm to solve for a subset of the output rows on the full inputs:
```
cd spmm_abrev
make profile -j
```

If you want to change parameters of the simulation (or even how many simulations to run):
1. open `spmm_abrev/Makefile` with your favorite editor
2. search for `CHANGE ME: TESTS`
3. you will some example tests being added to the `TESTS` lists
4. you can edit this list freely
