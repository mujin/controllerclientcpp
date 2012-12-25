MUJIN Controller C++ Client Library
-----------------------------------

This is an open-source client library communicating with the MUJIN Controller WebAPI.

Uses `libcurl <http://curl.haxx.se/libcurl/>`_ for communication. `C Bindings <http://curl.haxx.se/libcurl/c/>`_ and `C++ Bindings <http://www.curlpp.org>`_

Building on Windows
-------------------

1. Checkout the source code and install
 - Download and install `msysGit <http://code.google.com/p/msysgit/downloads/list?q=full+installer+official+git>`_
 - (Optional for nice graphical interface) Download and install  `TortoiseGit <http://code.google.com/p/tortoisegit/wiki/Download>`_ 
 - Checkout the following git repository **https://github.com/mujin/controllerclientcpp.git**

2. Download and install `CMake <http://www.cmake.org/cmake/resources/software.html>`_

3. `Download and install boost <http://www.boostpro.com/download/>`_ (any version >= 1.40 is fine).
 - Select Multi-threaded DLL libraries.
 - No extra libraries need to be selected, only the header files.
 
  There is a default included boost (v1.44) if one cannot be detected.

4. Run CMake on this directory, choose the correct Visual Studio version for the Generator.

.. image:: docs/build_cmake.png

5. Open the **build/mujincontrollerclientcpp.sln** solution and compile the **ALL_BUILD** project.

6. In order to Install into ``c:\Program Files``, compile the **INSTALL** project

Using Library
-------------

Once installed, the **mujincontrollerclient-vcXX-mt.dll** and **mujincontrollerclient-vcXX-mt.lib** will be generated.

Other Possible Clients
----------------------

- `cpp-netlib <http://cpp-netlib.github.com/latest/index.html>`_ - uses boost asio and cmake. `Using wiith https <https://groups.google.com/forum/?fromgroups=#!topic/cpp-netlib/M8LIz9ahMLo>`_ requires at least v0.9.4.

- `Windows HTTP Services <http://msdn.microsoft.com/en-us/library/aa384273%28VS.85%29.aspx?ppud=4>`_

- `libcurl.NET <http://sourceforge.net/projects/libcurl-net/>`_ - Windows only
