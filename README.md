# RtBot framework

Plataform to process real time series.

## Examples

See folder `examples/c++` (for **c++**) and `examples/notebook` (for **python**).

## Roadmap

- Core:

  - [ ] Create core framework in **C++**
  - [x] Generic operator setup
  - [ ] Read pipeline from yaml
  - Wrappers:

    - [x] **python**
    - [ ] **wasm**
    - [ ] **java**

    next iteration:

    - [ ] **go**
    - [ ] **rust**
    - [ ] **R**
    - [ ] **swift**

  - [ ] Standard library of common operators
  - [ ] Define "trainable" parameters in operator inputs
  - [ ] Implement optimization algorithm to find best values
        for parameters respecting its constraints

  next iteration:

  - [ ] Library of vital signal operators
  - [ ] Library of financial operators

- Frontend:

  - Account:

    - [ ] Login
    - [ ] Logout
    - [ ] Basic profile

  - [ ] Run RtBot yaml programs using the **wasm** api
  - Data:

    - [ ] Load data
    - [ ] Preview data
    - [ ] Delete data
    - [ ] Data navigator

    next iteration:

    - [ ] Edit data

  - [ ] Plot timeseries
  - [ ] Allow several plots in the same chart
  - [ ] Create programs using a yaml editor
  - [ ] Plot output of programs on top of data plots

  next iteration:

  - [ ] Interface to allow long running training jobs to find best
        parameters

- Backend:

  - [ ] User account
  - [ ] Define domain models and store data in postgres
  - [ ] Store uploaded data as file

  next iteration:

  - [ ] Allow users to run training jobs (RtBot program + data) to
        find best parameter values
  - [ ] Share data between users

- Documentation:
  - [ ] Use cases
  - [x] Jupyter notebook **python** example
  - [ ] General documentation of **RtBot** framework
  - [ ] Trading bot tutorial
  - [ ] Arduinio tutorial
  - [ ] ECG tutorial
  - [ ] Performance comparison with other approaches
