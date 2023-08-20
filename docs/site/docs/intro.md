---
sidebar_position: 1
---

# Introduction

RtBot is a general purpose digital signal processing framework focused on early event detection.
It features a new paradigm for signal processing inspired in digital circuits, focusing the timeseries
analysis in the time domain.
It’s written in c++ and has been built with performance in mind.

Wrapper libraries exist for python, javascript (using wasm) and rust, and in the future we plan
to support java and swift languages as well. In this sense RtBot can be easily adapted in any
existing codebase, as it already provides interfaces to the most popular existing computer
languages or new ones can be built for those missing languages using their native interface
with c++. RtBot programs are very fast and have a low footprint, being able to run even in
microcontrollers with very limited resources, such a raspberry pi pico or the esp32.

As a digital processing framework, it contains the common pieces found in such frameworks such as
high and low pass filters, finite response filters, signal re-samplers and so on.

As an event detector framework it is designed to give developers the tools to minimize the time
difference between the moment when the event is detected and the moment when the event actually
happened.
