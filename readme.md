### Build Setup 

- VS 2019 
- Latest DDK

### Build Instructions

- Specify the named pipe you want to monitor in NamedPipeInspectDrv/FltOps.h

- Build in debug or release for x64

- Specify an appropriate filter altitude

- Use OSR loader or whatever your fancy to load the driver as a minifilter driver

### Useage

- Load the driver

- Run NamedPipeInspect.exe as admin

`PS> NamedPipeInspect.exe | tee pipe.log`

### Note:

In testing an EDR product I found that the inspected r/w buffers contained data from previous r/w operations.

I'll have to do more testing to be sure, but testing multiple r/w with a named pipe echo server and another named pipe didn't reproduce the bug.

So it's probably just a mem leak for that one process?


