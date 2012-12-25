MUJIN Controller C++ Client Library
-----------------------------------

This is an open-source client library communicating with the MUJIN Controller WebAPI.

Uses `libcurl <http://curl.haxx.se/libcurl/>`_ for communication. `C Bindings <http://curl.haxx.se/libcurl/c/>` and `C++ Bindings <http://www.curlpp.org>`_

Building on Windows
-------------------

# Download and install `CMake <http://www.cmake.org/cmake/resources/software.html>`_

# `Download and install boost <http://www.boostpro.com/download/>`_ (any version >= 1.40 is fine).
 - Select Multi-threaded DLL libraries.
 - No extra libraries need to be selected, only the header files.
 
  There is a default included boost (v1.44) if one cannot be detected.

# Run CMake on this directory, choose the correct Visual Studio version for the Generator.

# Open the mujincontrollerclientcpp.sln solution and compile.

# In order to Install into "c:\Program Files", compile the "BUILD" project

Other Possible Clients
----------------------

- `cpp-netlib <http://cpp-netlib.github.com/latest/index.html>`_ - uses boost asio and cmake. `Using wiith https <https://groups.google.com/forum/?fromgroups=#!topic/cpp-netlib/M8LIz9ahMLo>`_ requires at least v0.9.4.

- `Windows HTTP Services <http://msdn.microsoft.com/en-us/library/aa384273%28VS.85%29.aspx?ppud=4>`_

- `libcurl.NET <http://sourceforge.net/projects/libcurl-net/>`_ - Windows only
