# nnimage
nnimage is a simple program designed to help create and manage disk images. Essentially, it serves as a tool to manage every part of disk image creation (including partitioning, formatting, and updating files) in a fast way that does not require root access. It has been designed with OS development as the first thing in mind.

## Dependencies
nnimage currently depends on libkrun/libkrunfw, as that it is the recommended way of isolating the disk image. Other backends are provided for ISO images (uses xorriso), loopback devices (requires root and generally not recommended),
and optionally libguestfs (which necessarily creates another dependency)

## Codebase AI policy
The current policy is that all contributions are judged on the actually code quality rather than wheter they are AI-generated or not. With that being said, vibe-coding entire components is heavily discouraged. Using AI assitance like AI auto complete, AI-generated test cases, and using AI to rough sketch out components and then subjecting the output to heavy scrutiny and review are all perfectly acceptable uses, but outright vibe-coding is greatly discouraged
