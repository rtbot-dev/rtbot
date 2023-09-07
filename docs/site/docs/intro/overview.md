# Overview

Programs are written as a composition of operators, which can receive and produce messages, an architecture inspired in
the actor model. It’s written in c++ and has been built with performance in mind. Wrapper libraries exist for python,
javascript (using wasm) and rust, and in the future we plan to support java and swift languages as well. In this sense
RtBot can be easily adapted in any existing codebase, as it already provides interfaces to the most popular existing
computer languages or new ones can be built for those missing languages using their native interface with c++. RtBot
programs are very fast and have a low footprint, being able to run even in microcontrollers with very limited resources,
such a raspberry pi pico or the esp32.
